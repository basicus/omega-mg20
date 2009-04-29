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
#include "../mg20func.h"

#define PORT         7701          /* default protocol port number */
#define QUEUE        20            /* size of request queue        */
#define NETWORK      "default"     /* default NETWORK name         */
#define PASSWORD     "cern"        /* password for default NETWORK */
#define MAX_THREADS  1000          /* maximum concurent threads    */
#define TIMEWAIT     30            /* TIME_WAIT interval           */
#define TIMEOUT      40            /* timeout for receive msg      */
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
void sig_exit(); /* signal handler for exit */
void sig_hup(); /* signal handler for HUP */

pthread_mutex_t  mut;              /* MUTEX for access to DATABASE */
pthread_attr_t attr;               /* attibute var for all threads */
size_t stacksize=32768;		   /* amount memory for thread     */
struct Omega OmegaThread[MAX_THREADS]; /* structure for new threads */
char *nw=NETWORK;
char *pw=PASSWORD;
int visits =  0;                   /* counter of client connections*/

main (int argc, char *argv[])
{
     struct   protoent  *ptrp;     /* pointer to a protocol table entry */
     struct   sockaddr_in sad;     /* structure to hold server's address */
     int      sd;                  /* socket descriptors */
     int      port;                /* protocol port number */
     int      alen;                /* length of address */
     int      th_r;		   /* Thread return */
     int      ci;		   /* current empty index */
     struct   timeval tv;          /* TIMEOUT for receive data from peer */


     pthread_mutex_init(&mut, NULL);
     pthread_attr_init(&attr);            /* init thread variables */
     pthread_attr_setstacksize (&attr, stacksize);
     tv.tv_sec = TIMEOUT;
     tv.tv_usec = 0 ;

     /* signal handlers */
     signal(SIGQUIT, &sig_exit);
     signal(SIGINT, &sig_exit);
     signal(SIGSEGV, &sig_exit);
     signal(SIGTERM, &sig_exit);
     signal(SIGABRT, &sig_exit);
     signal(SIGHUP, &sig_hup);

     memset((char  *)&sad,0,sizeof(sad)); /* clear sockaddr structure   */
     sad.sin_family = AF_INET;            /* set family to Internet     */
     sad.sin_addr.s_addr = INADDR_ANY;    /* set the local IP address   */

     /* Check  command-line argument for protocol port and extract      */

     if (argc > 1) {                        /* if argument specified     */
                     port = atoi (argv[1]); /* convert argument to binary*/
     } else {
                      port = PORT;     /* use default port number   */
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
     fprintf( stderr, "Server up and running.\n");
     NetPassword(1,0,nw,pw);

     while (1) {
    	 ci = -1; /* init current empty index */
	 /* select first empty thread id */
	 while (ci<0)
	    {
		ci=s_empty();
		if ( ci <0 ) { printf ("There is no enough resources to accept new threads"); sleep (10);}
	    }
         printf("SERVER: Waiting for connect to thread %d...\n",ci);

         if (  (OmegaThread[ci].sd=accept(sd, (struct sockaddr *)&OmegaThread[ci].ds, &alen)) < 0) {
	                      fprintf(stderr, "accept failed\n");
                              exit (1);
	 }

	if ( setsockopt (OmegaThread[ci].sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv)) printf("setsockopt error (SO_RCVTIMEO)\n");

	/* locking thread to that connection and setting than he is busy */
        pthread_mutex_lock(&mut);
           OmegaThread[ci].st=1;
        pthread_mutex_unlock(&mut);

	printf("Client connection from %s\n", inet_ntoa(OmegaThread[ci].ds.sin_addr));

	th_r = pthread_create(&OmegaThread[ci].td, &attr, serverthread, (void *) ci );
	if ( th_r != 0 ) {
	  printf ("The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process PTHREAD_THREADS_MAX would be exceeded.\n");
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
   int r1;          /* counter of current receive */
   int n;           /* tail */
   iconv_t win2utf;

   c = (int) parm;
   tsd = OmegaThread[c].sd;
   win2utf = iconv_open ("UTF-8", "CP1251"); // осуществляем перекодировку из CP1251 в кодировку локали (UTF-8)
   if (win2utf == (iconv_t) -1)
         { printf ("Can't converse from '%s' to wchar_t not available\n","CP1251"); }


   pthread_mutex_lock(&mut);
       tvisits=++visits;
   pthread_mutex_unlock(&mut);

   printf("SERVER thread[%d] accepted connection number %d\n", c,tvisits);
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
                if ((nconv == (size_t) -1) & (errno == EINVAL)) { printf ("Error! Can't convert some input text\n"); }
                *rconv='\0';
                printf ("<SRC=%d DST=%d>%s\n",s,d,conv);
                if ( OmegaThread[c].st==1 && NetAuth(nw,pw,conv) ==1) { /* check if client not auth */
                        pthread_mutex_lock(&mut);
                        OmegaThread[c].st=2;
                        pthread_mutex_unlock(&mut);
                } else {
                 printf ("ERROR client auth thread %d\n",c);
                 cs=3; /* setting status of closed socket */
                 break; /* exiting from loop of receive message */
                }

                /* ... SOME code for processing message ... */
                if ( r >0 ) {memcpy(buffer, buffer+n, r);} /* copy message from the end to begin */
                        else {memset(buffer,0x00,n);}
            }
        } else /* exit with timeout */
        {
            printf ("ERROR reading from socket thread %d (TIMEOUT)\n",c);
            cs=1; /* setting status of closed socket */
            break; /* exiting from loop of receive message */
        }
   }

   if (r>BUF_SIZE-256) { cs=2; printf ("ERROR buffer overflow of thread %d. May be problem with connection?\n",c); }

   /* end of code processing */
   printf("SERVER thread[%d] closing connection number %d with status %d\n", c,tvisits,cs);
   close(tsd);

   pthread_mutex_lock(&mut);
       OmegaThread[c].st=0;
   pthread_mutex_unlock(&mut);

   pthread_exit(0);
}

void sig_hup()
{
    signal(SIGHUP,&sig_hup); /* сборс сигнала */
    printf("Received HUP signal.\n");

}


void sig_exit()
{
    printf("\nExiting...\n");
    exit(0);
}
