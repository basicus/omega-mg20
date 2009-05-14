/* compile: gcc -o omega_server omega_server.c -lpthread
 Omega MG20 TCP server (daemon)
 Syntax:         omega_server [port] [ {NETWORK PASSWORD } ]
 Note:           The port, network and password argument is optional.
 (c) Sergey Butenin, 2009
*/
#define NET          2
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include "../mg20func.h"


#define PORT         7701          /* default protocol port number */
#define QUEUE        20            /* size of request queue        */
#define NETWORK      "default"     /* default NETWORK name         */
#define PASSWORD     "cern"        /* password for default NETWORK */
#define MAX_THREADS  2000          /* maximum concurent threads    */
#define TIMEWAIT     30            /* TIME_WAIT interval           */
#define TIMEOUT      240            /* timeout for receive msg      */
#define BUF_SIZE     1024          /* size of receive buffer       */
#define DAEMON_NAME "omega_server"
#define PID_FILE "/tmp/omega_server.pid"
#define CTL_SOCKET "/tmp/omega.ctl"
#define QLEN_UNIX 10
#define SRV_ADDR 65534              /* server address on the network */


//# TODO! Add support for conversion to windows 1251 (from UTF8)
//# TODO! Add support for mutex, when accessing array of sockets;


struct Omega {
    struct sockaddr_in ds;                /* destination address          */
    pthread_t   td;                /* thread id                    */
    int         sd;                /* socket descriptor            */
    int         st;                /* status of connection         */
    int	        ad;                /* Omega address                */
    int	        nw;		   /* Network id                   */
};

void * serverthread(void * parm);  /* thread function              */
int s_empty();
void signal_handler(int sig);
void PrintUsage(int argc, char *argv[]); /* Print usage */
void print_msg (char *msg);
void control_socket ();
int isAlive (unsigned short d);

pthread_mutex_t  mut;              /* MUTEX for access to DATABASE */
pthread_attr_t attr;               /* attibute var for all threads */
size_t stacksize=32768;		   /* amount memory for thread     */
struct Omega OmegaThread[MAX_THREADS]; /* structure for new threads */
char *nw=NULL;
char *pw=NULL;
unsigned short s_hub=SRV_ADDR;
int visits =  0;                   /* counter of client connections*/
int daemonize;                     /* if 1 then daemonize, else console */
int port = PORT;     /* use default port number   */
char *msg;
char* socket_name = CTL_SOCKET; /* UNIX socket name */
char* pid_file = PID_FILE;     /* PID file */
int      sd;                  /* socket descriptors */
int ctl_connected=0;          /* flag of active control connection */
int c_fd; /* control connection file descriptor */

main (int argc, char *argv[])
{
     struct   protoent  *ptrp;     /* pointer to a protocol table entry */
     struct   sockaddr_in sad;     /* structure to hold server's address */
     int      alen;                /* length of address */
     int      th_r;		   /* Thread return */
     int      ci;		   /* current empty index */
     struct   timeval tv;          /* TIMEOUT for receive data from peer */
     pid_t pid, sid;
     int      c_th;          /* Control thread */
     pthread_t   c_td;
     msg = malloc(256);
     pthread_mutex_init(&mut, NULL);
     pthread_attr_init(&attr);            /* init thread variables */
     pthread_attr_setstacksize (&attr, stacksize);
     tv.tv_sec = TIMEOUT;
     tv.tv_usec = 0 ;

     /* signal handlers */
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

     memset((char  *)&sad,0,sizeof(sad)); /* clear sockaddr structure   */
     sad.sin_family = AF_INET;            /* set family to Internet     */
     sad.sin_addr.s_addr = INADDR_ANY;    /* set the local IP address   */

     /* Check  command-line argument for protocol port and extract      */
    int c;
    nw = NETWORK;
    pw = PASSWORD;

    while( (c = getopt(argc, argv, "dhp:P:n:")) != -1) {
        switch(c){
            case 'h':
                PrintUsage(argc, argv);
                exit(0);
                break;
            case 'd':
                /* daemonize flag */
                daemonize = 1;
                //printf ("Setting daemonizing...\n");
                break;
            case 'p':
                /* set TCP port */
                port = atoi (optarg);
                if (port==0) { port = PORT; }
                //printf ("Setting port %d\n",port);
                break;
            case 'n':
                /* set network */
                nw = optarg;
                //printf ("Setting network \"%s\"\n",nw);
                break;
            case 'P':
                /* set password of network */
                pw = optarg;
                //printf ("Setting password \"%s\"\n",pw);
                break;
            default:
                PrintUsage(argc, argv);
                exit(0);
                break;
        }
    }




    if (daemonize==1) {
        syslog(LOG_INFO, "Starting the daemonizing process");

        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        int fpid = open(pid_file, O_WRONLY | O_CREAT , 0640);
        if ( fpid < 0) { print_msg("Error! Failed to create PID file");
            exit(EXIT_FAILURE);
            } else {
            char *s_pid;
            s_pid = (char *) malloc(6);
            sprintf (s_pid, "%d\n", getpid());
            int rpid = write (fpid, s_pid, strlen(s_pid));
            if (rpid < 0) {
                print_msg ("Error! Cant write to PID file.");
                exit(EXIT_FAILURE);
                }
            }
        close (fpid);
    }

     if (port > 0)                          /* test for illegal value    */
                      sad.sin_port = htons((u_short)port);
     else {                                /* print error message and exit */
                      fprintf (stderr, "Error: bad port number %s/n",argv[1]);
                      exit (1);
     }

     /* Map TCP transport protocol name to protocol number */

     if ( ((int)(ptrp = getprotobyname("tcp"))) == 0)  {
                     fprintf(stderr, "Error: cannot map \"tcp\" to protocol number");
                     exit (1);
     }

     /* Create a socket */
     sd = socket (PF_INET, SOCK_STREAM, ptrp->p_proto);
     if (sd < 0) {
                       fprintf(stderr, "Error: socket creation failed\n");
                       exit(1);
     }

     /* set sockopt for reuse PORT */
    #ifdef SO_REUSEPORT
     int yes=1;
     if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        fprintf("Error: setsockopt(SO_REUSEADDR)\n");
        close(sd);
	exit(1);
    }
   #endif

    #ifdef SO_LINGER
    struct linger l = { 1, TIMEWAIT };
    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &l, sizeof(struct linger)) == -1) {
        //fprintf("setsockopt(SO_LINGER) error\n");
	close (sd);
	exit(1);
	}
    #endif


     /* Bind a local address to the socket */
     if (bind(sd, (struct sockaddr *)&sad, sizeof (sad)) < 0) {
                        fprintf(stderr,"Error: bind to port failed\n");
                        exit(1);
     }

     /* Specify a size of request queue */
     if (listen(sd, QUEUE) < 0) {
                        fprintf(stderr,"Error: listen failed\n");
                         exit(1);
     }

     alen = sizeof(sad);

    /* Create thread for contol socket */
    th_r = pthread_create(&c_td, &attr, control_socket, NULL );

    if ( th_r != 0 ) { print_msg("Error! Can't create thread for control socket! Continue without CTL."); }

     /* Main server loop - accept and handle requests */
     if ( daemonize ==1 ) { syslog(LOG_INFO, "%s daemon starting up", DAEMON_NAME);
                            setlogmask(LOG_UPTO(LOG_DEBUG));
                            openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
                            }
    print_msg("Omega server up and running.");

    /* Our process ID and Session ID */

    //NetPassword (2200, 65534, "default", "cern");
     while (1) {
    	 ci = -1; /* init current empty index */
	 /* select first empty thread id */
	 while (ci<0)
	    {
		ci=s_empty();
		if ( ci <0 ) { print_msg("Warning! There is no empty threads!"); sleep (10);}
	    }
         sprintf(msg,"Waiting for connect to thread number %d...",ci);
         print_msg(msg);

         if (  (OmegaThread[ci].sd=accept(sd, (struct sockaddr *)&OmegaThread[ci].ds, &alen)) < 0) {
	                      sprintf(msg, "Error! accept new connection failed");
	                      print_msg(msg);
                          exit (1);
	 }

	if ( setsockopt (OmegaThread[ci].sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv)) { print_msg("Error! setsockopt (SO_RCVTIMEO)"); }

	/* locking thread to that connection and setting than he is busy */
        pthread_mutex_lock(&mut);
           OmegaThread[ci].st=1;
        pthread_mutex_unlock(&mut);

	sprintf(msg,"Connection from %s address.", inet_ntoa(OmegaThread[ci].ds.sin_addr));
	print_msg(msg);

	th_r = pthread_create(&OmegaThread[ci].td, &attr, serverthread, (void *) ci );
	if ( th_r != 0 ) {
	  sprintf (msg,"Warning! The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process PTHREAD_THREADS_MAX would be exceeded.");
	  print_msg(msg);
	}

     }
     close(sd);
}

/* search first empty element of array */
int s_empty()
{
 int z;
 for(z=0; z < MAX_THREADS; z++)
    {
    if ( OmegaThread[z].st==0 )
    	    {return z;}
     }
 return -1;
}

/* функция обработки соединений */
void * serverthread(void * parm)
{
   int tsd, tvisits, c;
   int cs=0;                    /* close status */
   char *reply=(char *) malloc(BUF_SIZE); /* receive buffer */
   char *rreply;                         /* pointer to rcv buffer */
   size_t sreply,sconv=2*BUF_SIZE,nconv; /* conversion from cp1251 to utf8 */
   char *conv=(char *)  malloc(2*BUF_SIZE),*rconv; /* for conversion buffer */
   unsigned char buf[BUF_SIZE];
   char *c_buf=(char *)malloc(300); /* buffer for control connection messages */
   unsigned char *buffer=(unsigned char*)&buf;
   unsigned short s;  /* src address */
   unsigned short d;  /* dst address */
   unsigned short r=0; /* counter of received buffer */
   char *ver="VERSION",*v_req; /* test message, version */
   int r1;          /* counter of current receive */
   int n;           /* tail */
   int l;
   iconv_t win2utf;

   c = (int) parm;
   tsd = OmegaThread[c].sd;
   win2utf = iconv_open ("UTF-8", "CP1251"); // осуществляем перекодировку из CP1251 в кодировку локали (UTF-8)
   if (win2utf == (iconv_t) -1)
         { sprintf (msg,"Error! Can't converse from '%s' to wchar_t not available","CP1251"); print_msg(msg); }

    v_req = (char *) malloc(50);

   pthread_mutex_lock(&mut);
       tvisits=++visits;
   pthread_mutex_unlock(&mut);

   sprintf(msg,"Thread[%d] accepted connection number %d\n", c,tvisits);
   print_msg(msg);
   /* code for processing client messages */
   while (r<BUF_SIZE-256) {
       r1 =  read( tsd, buffer+r, 256 );

       if ( r1==1 && buffer[r]==0 && OmegaThread[c].st==2 ) { /* received KA packet */
           sprintf(msg,"Thread %d received KA packet from %d", c, OmegaThread[c].ad);
           print_msg(msg);
           }

       if ( r1 >= 1 ) { /* received some portion of data and not KA */
	          r = r +r1;
              #ifdef DEBUG
	           int i;
               printf ("Readed some info %d:%d\n",r1,r);
               for(i=0; i<r1; i++) printf("%02x",(unsigned char)buffer[i]);
               printf ("\n");
	          #endif
            /* decoding message */
            while ( ParseBufferMessageNet(&s,&d,reply,buffer,r,&n)==0 ) { /* if message correct decoded */
                r = r - n; /* determine tail of undecoded message */
                /* convert message from cp1251 to UTF8 */
                sreply=strlen(reply);
                rreply=reply;
                rconv=conv;
                sconv=2*BUF_SIZE;
                nconv = iconv (win2utf, &rreply, &sreply, &rconv, &sconv);
                if ((nconv == (size_t) -1) & (errno == EINVAL)) { print_msg ("Error! Can't convert some input text"); }
                *rconv='\0';
                sprintf (msg,"Thread %d received msg from %d to %d:%s",c,s,d,conv); /* may be show if debug? */
                print_msg(msg);
                /* Checks */
                if ( OmegaThread[c].st==1 && NetAuth(nw,pw,conv) ==1 ) { /* check if client not auth-ed before  */
                        if ( isAlive(s) ==-1 ) {
                            pthread_mutex_lock(&mut); /* locking for a while */
                            OmegaThread[c].st=2;
                            OmegaThread[c].ad=s;
                            OmegaThread[c].nw=1;
                            pthread_mutex_unlock(&mut);

                            sprintf (msg,"Thread %d client authentication successfull. Setting thread client address %d",c,s);
                            print_msg(msg);

                            if (ctl_connected==1) { /* if control connection alive - sending EVENT to it */
                                sprintf (c_buf,"LOGON %d\n",OmegaThread[c].ad);
                                write(c_fd,c_buf,strlen(c_buf));
                            }

                            print_msg("Sending \'version\' request command");
                            l=CreateTextMessageNet (&s_hub, &OmegaThread[c].ad, ver,v_req);
                            write(OmegaThread[c].sd,v_req,l);
                            break;
                        } else {
                            sprintf (msg,"Thread %d client duplicate authorization, closing connection",c);
                            print_msg(msg);
                            cs=3; /* setting status of closed socket */
                            break; /* exiting from loop of receive message */
                        }
                } else if (OmegaThread[c].st==1)  {
                 sprintf (msg,"Thread %d client authentication failed!",c);
                 print_msg(msg);
                 cs=3; /* setting status of closed socket */
                 break; /* exiting from loop of receive message */
                }

                if (OmegaThread[c].st==2) { /* Client already authenticated */
                    //print_msg("Client connected, now we decide to do some checks");
                    if ( d == s_hub ) { /* message send to HUB (us), resending it to control channel */
                        if ( ctl_connected ==1) {
                        sprintf (c_buf,"RCV %d %s\n",OmegaThread[c].ad,conv);
                        write(c_fd,c_buf,strlen(c_buf));
                        }
                    } else if ( isAlive(d) >= 0 ) { /* Checking if device is connected and auth*/
                        l=CreateTextMessageNet (&s, &d, conv,reply);
                        write(OmegaThread[isAlive(d)].sd,reply,l);
                    } else {
                        sprintf (msg,"Thread %d warning, device address %d isn't connected",c,d);
                        print_msg(msg);
                    }
                }

                if ( r >0 ) {memcpy(buffer, buffer+n, r);} /* copy message from the end to begin */
                        else {memset(buffer,0x00,n);}
                }
        } else /* exit with timeout */
        {
            sprintf (msg,"Thread %d error reading from socket. Timeout or auth error",c);
            print_msg(msg);
            cs=1; /* setting status of closed socket */
            break; /* exiting from loop of receive message */
        }

   }

   if (r>BUF_SIZE-256) { cs=2; sprintf (msg,"Thread %d buffer overflow. May be problem with connection?",c); print_msg(msg); }

   /* end of code processing */
   sprintf(msg,"Thread %d closing connection number %d with status %d", c,tvisits,cs);
   print_msg(msg);
   if (ctl_connected==1) {
                            sprintf (c_buf,"LOGOFF %d\n",OmegaThread[c].ad);
                            write(c_fd,c_buf,strlen(c_buf));
                            }
   close(tsd);

   pthread_mutex_lock(&mut);
       OmegaThread[c].st=0;
   pthread_mutex_unlock(&mut);

   pthread_exit(0);
}

void control_socket ()
{

    int s_fd;  /* Server listen socket */
    struct sockaddr_un uname;
    struct sockaddr_un client_name;
    socklen_t client_name_len;
    int length=-1;
    char *command;
    char *bcommand,*ecommand, *c_print,*cc,*t_msg,*s_buf;
    int c_len,e_len;
    int parsed; /* variable to hold number parsed */
    unsigned short dst; /* dst address */
    int l;
    int iclose=0;

    /* allocate memory for buffer */
    command = (char *) malloc(BUF_SIZE);
    c_print = (char *) malloc(BUF_SIZE);
    cc = (char *) malloc (BUF_SIZE);
    t_msg = (char *) malloc(256);
    s_buf = (char *) malloc(256);

    /* Prepary server socket */
    s_fd = socket(PF_LOCAL, SOCK_STREAM,0);
    uname.sun_family = AF_LOCAL;
    strcpy(uname.sun_path, socket_name);

    /* Binding */
    bind(s_fd, &uname, SUN_LEN(&uname));

    /* Listen for connections */
    listen (s_fd,QLEN_UNIX);
    print_msg("Created control socket...");
    iclose =0;
    while (1) {
	/* accepting control connection */
        c_fd = accept ( s_fd, &client_name, &client_name_len);
        print_msg("Accepted control connection.");
        ctl_connected = 1; /* setting flag of control connection */

	    do {

	    /* repeately read from socket */
        length = read (c_fd, command, BUF_SIZE);
        if ( length == 0 ) { ctl_connected = 0; break;}
        /*sprintf(cc,"Received command:%s, %d",command,length);
        command[length]='\0';
        print_msg(cc);*/
        command[length]='\0';
        e_len = 0;
        bcommand = command;
        ecommand = memchr(command,'\n',length);
        while ( ecommand != NULL ) {
            c_len = (ecommand - bcommand) + 1;
            memcpy(c_print,bcommand,c_len);
            c_print[c_len-1]='\0';
            bcommand = &ecommand[1];
            sprintf(cc,"Received command:%s",c_print);
            print_msg(cc);
            /* START of message process block */
            if ( strcmp(c_print,"QUIT")==0 )  { print_msg("Received quit command on control channel"); iclose=1; break; }
            if ( strncmp (c_print,"SEND ", 5) == 0 ) {  /* send message to address */
                parsed = sscanf (c_print,"SEND %5hu %256s",&dst,t_msg);
                if ( parsed == 2) {
                    if ( isAlive(dst) >= 0 ) {
                        sprintf(cc,"Sending message to %d device",dst);
                        print_msg(cc);
                        l=CreateTextMessageNet (&s_hub, &dst, t_msg,s_buf);
                        write(OmegaThread[isAlive(dst)].sd,s_buf,l);
                    } else {
                        print_msg("Warning, device isn't registered. Not sending");
                    }
                }
            }
            /* END of message process block */
            e_len+=c_len;
            if ( e_len>= length) {
                ecommand=NULL;
            } else ecommand = memchr(bcommand,'\n',length-e_len);
        }
        } while (length!=0 || iclose==0);

    print_msg("Closing control connection.");
    ctl_connected = 0;
    close (c_fd);
   }
   free(command);
   close (s_fd);
}


void PrintUsage(int argc, char *argv[]) {
    if (argc >=1) {
        printf("Usage: %s -h -n\n", argv[0]);
        printf("  Options:\n");
        printf("      -d\tFork as a daemon.\n");
        printf("      -h\tShow this help screen.\n");
        printf("n");
    }
}

int isAlive (unsigned short d) {
int i;
 for (i = 0; i < MAX_THREADS; i++ ) {
       if ( OmegaThread[i].st==2 && OmegaThread[i].ad==d ) return i;
 }
   return -1;
}
void signal_handler(int sig) {

    switch(sig) {
        case SIGHUP:
            print_msg("Received SIGHUP signal.");
            close(sd);
            unlink(socket_name);
            unlink(pid_file);
            break;
        case SIGTERM:
            print_msg("Received SIGTERM signal.");
            close(sd);
            unlink(socket_name);
            unlink(pid_file);
            exit (0);
            break;
        case SIGQUIT:
            print_msg("Received SIGQUIT signal.");
            close(sd);
            unlink(socket_name);
            unlink(pid_file);
            exit (0);
            break;
        case SIGINT:
            print_msg("Received SIGINT signal.");
            close(sd);
            unlink(socket_name);
            unlink(pid_file);
            exit (0);
            break;
        case SIGABRT:
            print_msg("Received SIGABRT signal.");
            close(sd);
            unlink(socket_name);
            unlink(pid_file);
            exit (0);
            break;
        case SIGSEGV:
            print_msg("Received SIGSEGV signal.");
            unlink(socket_name);
            close(sd);
            unlink(pid_file);
            exit (0);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d)", strsignal(sig));
            break;

    }
}

void print_msg (char *msg) {
if (daemonize==1) { syslog(LOG_WARNING, "%s",msg); }
   else {
        time_t rawtime;
        struct tm * timeinfo;
        char *ctime=(char *) malloc(128);
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        strftime (ctime,128,"%d.%m.%Y %H:%M:%S",timeinfo);
        printf ("%s %s\n",ctime,msg);
       }
}
