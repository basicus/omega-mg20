/* compile: gcc -o omega_server omega_server.c -lpthread
 Omega MG20 TCP server (daemon)
 Syntax:         omega_server [port] [ {NETWORK PASSWORD } ]
 Note:           The port, network and password argument is optional.
 (c) Sergey Butenin, 2009
*/
#define NET          2
#include <sys/types.h>
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
#define DAEMON_NAME "omega_server"
#define PID_FILE "/var/run/omega_server.pid"


#define PORT         7701          /* default protocol port number */
#define QUEUE        20            /* size of request queue        */
#define NETWORK      "default"     /* default NETWORK name         */
#define PASSWORD     "cern"        /* password for default NETWORK */
#define MAX_THREADS  1000          /* maximum concurent threads    */
#define TIMEWAIT     30            /* TIME_WAIT interval           */
#define TIMEOUT      240            /* timeout for receive msg      */
#define BUF_SIZE     1024          /* size of receive buffer       */




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
//void print_msg (char *msg);
pthread_mutex_t  mut;              /* MUTEX for access to DATABASE */
pthread_attr_t attr;               /* attibute var for all threads */
size_t stacksize=32768;		   /* amount memory for thread     */
struct Omega OmegaThread[MAX_THREADS]; /* structure for new threads */
char *nw=NULL;
char *pw=NULL;
unsigned short s_hub=65534;
int visits =  0;                   /* counter of client connections*/
int daemonize;                     /* if 1 then daemonize, else console */
int port = PORT;     /* use default port number   */
char *msg;

main (int argc, char *argv[])
{
     struct   protoent  *ptrp;     /* pointer to a protocol table entry */
     struct   sockaddr_in sad;     /* structure to hold server's address */
     int      sd;                  /* socket descriptors */
     int      alen;                /* length of address */
     int      th_r;		   /* Thread return */
     int      ci;		   /* current empty index */
     struct   timeval tv;          /* TIMEOUT for receive data from peer */
     pid_t pid, sid;

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
                printf ("Daemonizing...\n");
                break;
            case 'p':
                /* set TCP port */
                port = atoi (optarg);
                if (port==0) { port = PORT; }
                printf ("Setting port %d\n",port);
                break;
            case 'n':
                /* set network */
                nw = optarg;
                printf ("Setting network \"%s\"\n",nw);
                break;
            case 'P':
                /* set password of network */
                pw = optarg;
                printf ("Setting password \"%s\"\n",pw);
                break;
            default:
                PrintUsage(argc, argv);
                exit(0);
                break;
        }
    }


     if (port > 0)                          /* test for illegal value    */
                      sad.sin_port = htons((u_short)port);
     else {                                /* print error message and exit */
                      fprintf (stderr, "bad port number %s/n",argv[1]);
                      exit (1);
     }

    /* Map TCP transport protocol name to protocol number */

     if ( ((int)(ptrp = getprotobyname("tcp"))) == 0)  {
                     fprintf(stderr, "cannot map \"tcp\" to protocol number");
                     exit (1);
     }

     /* Create a socket */
     sd = socket (PF_INET, SOCK_STREAM, ptrp->p_proto);
     if (sd < 0) {
                       fprintf(stderr, "socket creation failed\n");
                       exit(1);
     }

     /* set sockopt for reuse PORT */
    #ifdef SO_REUSEPORT
     int yes=1;
     if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
        fprintf("setsockopt(SO_REUSEPORT) error\n");
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
                        fprintf(stderr,"bind failed\n");
                        exit(1);
     }

     /* Specify a size of request queue */
     if (listen(sd, QUEUE) < 0) {
                        fprintf(stderr,"listen failed\n");
                         exit(1);
     }

     alen = sizeof(sad);

     /* Main server loop - accept and handle requests */
     if ( daemonize ==1 ) { syslog(LOG_INFO, "%s daemon starting up", DAEMON_NAME);
                            setlogmask(LOG_UPTO(LOG_DEBUG));
                            openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
                            }
            else fprintf( stderr, "Server up and running.\n");

    /* Our process ID and Session ID */


    if (daemonize==1) {
        syslog(LOG_INFO, "starting the daemonizing process");

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

        /*
        int fpid = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if ( fpid < 0) { print_msg("Eror! Failed to create PID file");
            exit(EXIT_FAILURE);
            } else {
            char pid[6];
            sprintf (pid, sizeof(pid), "%d\n", getpid());
            int rpid = write (fpid, pid, strlen(pid));
            if (rpid < 0) {
                print_msg ("Error! Cant write to PID file.");
                exit(EXIT_FAILURE);
                }
            }
        close (fpid);
        */
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
    }


     while (1) {
    	 ci = -1; /* init current empty index */
	 /* select first empty thread id */
	 while (ci<0)
	    {
		ci=s_empty();
		if ( ci <0 ) { print_msg("Thereis no empty threads"); sleep (10);}
	    }
         sprintf(msg,"SERVER: Waiting for connect to thread %d...",ci);
         print_msg(msg);

         if (  (OmegaThread[ci].sd=accept(sd, (struct sockaddr *)&OmegaThread[ci].ds, &alen)) < 0) {
	                      sprintf(msg, "accept failed");
	                      print_msg(msg);
                          exit (1);
	 }

	if ( setsockopt (OmegaThread[ci].sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv)) { print_msg("setsockopt error (SO_RCVTIMEO)"); }

	/* locking thread to that connection and setting than he is busy */
        pthread_mutex_lock(&mut);
           OmegaThread[ci].st=1;
        pthread_mutex_unlock(&mut);

	sprintf(msg,"Client connection from %s", inet_ntoa(OmegaThread[ci].ds.sin_addr));
	print_msg(msg);

	th_r = pthread_create(&OmegaThread[ci].td, &attr, serverthread, (void *) ci );
	if ( th_r != 0 ) {
	  sprintf (msg,"The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process PTHREAD_THREADS_MAX would be exceeded.");
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
   unsigned char *buffer=(unsigned char*)&buf;
   unsigned short s;  /* src address */
   unsigned short d;  /* dst address */
   unsigned short r=0; /* counter of received buffer */
   char *msg2="Sm>P=1?"; /* test message */
   int r1;          /* counter of current receive */
   int n;           /* tail */
   int l;
   iconv_t win2utf;

   c = (int) parm;
   tsd = OmegaThread[c].sd;
   win2utf = iconv_open ("UTF-8", "CP1251"); // осуществляем перекодировку из CP1251 в кодировку локали (UTF-8)
   if (win2utf == (iconv_t) -1)
         { sprintf (msg,"Can't converse from '%s' to wchar_t not available","CP1251"); print_msg(msg); }


   pthread_mutex_lock(&mut);
       tvisits=++visits;
   pthread_mutex_unlock(&mut);

   sprintf(msg,"SERVER thread[%d] accepted connection number %d\n", c,tvisits);
   print_msg(msg);
   /* code for processing client messages */
   while (r<BUF_SIZE-256) {
       r1 =  read( tsd, buffer+r, 256 );
       if ( r1 > 0 ) { /* received some portion of data */
	          r = r +r1;
              #ifdef DEBUG
	           int i;
               printf ("Readed some info %d:%d\n",r1,r);
               for(i=0; i<r1; i++) printf("%02x",(unsigned char)buffer[i]);
               printf ("\n");
	          #endif
      /* decoding message */
            while ( ParseBufferMessageNet(&s,&d,reply,buffer,r,&n)==0 ) { /* if message correct decoded */
                r = r - n; /* determine tail undecoded message */
                /* convert message from cp1251 to UTF8 */
                sreply=strlen(reply);
                rreply=reply;
                rconv=conv;
                sconv=2*BUF_SIZE;

                nconv = iconv (win2utf, &rreply, &sreply, &rconv, &sconv);
                if ((nconv == (size_t) -1) & (errno == EINVAL)) { print_msg ("Error! Can't convert some input text"); }
                *rconv='\0';
                sprintf (msg,"Received msg:<SRC=%d DST=%d>%s [%d:%d]\n",s,d,conv,c,OmegaThread[c].st);
                print_msg(msg);
                if ( OmegaThread[c].st==1 && NetAuth(nw,pw,conv) ==1 ) { /* check if client not auth */
                        pthread_mutex_lock(&mut);
                        OmegaThread[c].st=2;
                        OmegaThread[c].ad=s;
                        OmegaThread[c].nw=1;
                        sprintf (msg,"Client auth successfull thread:%d. Address %d\n",c,s);
                        print_msg(msg);
                        pthread_mutex_unlock(&mut);
                        print_msg("Sending test msg");
                        l=CreateTextMessageNet (&s_hub, &OmegaThread[c].ad, msg2,reply);
                        write(OmegaThread[c].sd,reply,l);

                } else if (OmegaThread[c].st==1)  {
                 sprintf (msg,"ERROR client auth thread %d\n",c);
                 print_msg(msg);
                 cs=3; /* setting status of closed socket */
                 break; /* exiting from loop of receive message */
                }


                /* ... SOME code for processing message ... */
                if ( r >0 ) {memcpy(buffer, buffer+n, r);} /* copy message from the end to begin */
                        else {memset(buffer,0x00,n);}
            }
        } else /* exit with timeout */
        {
            sprintf (msg,"ERROR reading from socket thread %d (TIMEOUT)\n",c);
            print_msg(msg);
            cs=1; /* setting status of closed socket */
            break; /* exiting from loop of receive message */
        }
   }

   if (r>BUF_SIZE-256) { cs=2; sprintf (msg,"ERROR buffer overflow of thread %d. May be problem with connection?\n",c); print_msg(msg); }

   /* end of code processing */
   sprintf(msg,"SERVER thread[%d] closing connection number %d with status %d\n", c,tvisits,cs);
   print_msg(msg);
   close(tsd);

   pthread_mutex_lock(&mut);
       OmegaThread[c].st=0;
   pthread_mutex_unlock(&mut);

   pthread_exit(0);
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

void signal_handler(int sig) {

    switch(sig) {
        case SIGHUP:
            print_msg("Received SIGHUP signal.");
            //remove(PID_FILE);
            break;
        case SIGTERM:
            print_msg("Received SIGTERM signal.");
            //remove(PID_FILE);
            exit (0);
            break;
        case SIGQUIT:
            print_msg("Received SIGQUIT signal.");
            //remove(PID_FILE);
            exit (0);
            break;
        case SIGINT:
            print_msg("Received SIGINT signal.");
            //remove(PID_FILE);
            exit (0);
            break;
        case SIGABRT:
            print_msg("Received SIGABRT signal.");
            //remove(PID_FILE);
            exit (0);
            break;
        case SIGSEGV:
            print_msg("Received SIGSEGV signal.");
            //remove(PID_FILE);
            exit (0);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d) %s", strsignal(sig));
            break;

    }
}

void print_msg (char *msg) {
 if (daemonize==1) { syslog(LOG_WARNING, "%s",msg); }
   else { printf ("%s\n",msg);}
}

