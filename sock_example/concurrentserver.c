/* compile: gcc -o omega_server omega_server.c -lpthread 
 Omega MG20 TCP server (daemon)  
 Syntax:         omega_server [port] [ {NETWORK PASSWORD } ]
 Note:           The port, network and password argument is optional.
 (c) Sergey Butenin, 2009
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define PORT         7701          /* default protocol port number */
#define QUEUE        20            /* size of request queue        */
#define NETWORK      "default"     /* default NETWORK name         */
#define PASSWORD     "cern"        /* password for default NETWORK */
#define MAX_THREADS  1000          /* maximum concurent threads    */
#define TIMEWAIT     30            /* TIME_WAIT interval           */

struct Omega {
    struct sockaddr_in ds;                /* destination address          */
    pthread_t   td;                /* thread id                    */
    int         sd;                /* socket descriptor            */
    int         st;                /* status of connection         */
    int	        ad;                /* Omega address                */
    int	        nw;		   /* Network id                   */
};

void * serverthread(void * parm);  /* thread function              */
pthread_mutex_t  mut;              /* MUTEX for access to DATABASE */
pthread_attr_t attr;               /* attibute var for all threads */
size_t stacksize=32768;		   /* amount memory for thread     */
struct Omega OmegaThread[MAX_THREADS]; /* structure for new threads */

int visits =  0;                   /* counter of client connections*/

main (int argc, char *argv[])
{
     struct   hostent   *ptrh;     /* pointer to a host table entry */
     struct   protoent  *ptrp;     /* pointer to a protocol table entry */
     struct   sockaddr_in sad;     /* structure to hold server's address */
//     struct   sockaddr_in cad;     /* structure to hold client's address */
     int      sd;                  /* socket descriptors */
     int      port;                /* protocol port number */
     int      alen;                /* length of address */
     int      th_r;		   /* Thread return */
     int      ci;		   /* current empty index */
//     pthread_t  tid;             /* variable to hold thread ID */

     pthread_mutex_init(&mut, NULL);
     pthread_attr_init(&attr);            /* init thread variables */
     pthread_attr_setstacksize (&attr, stacksize); 


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

	/* locking thread to that connection */
        pthread_mutex_lock(&mut);
           OmegaThread[ci].st=1;
        pthread_mutex_unlock(&mut);
	 
	 printf("Client connected [%s]\n", inet_ntoa(OmegaThread[ci].ds.sin_addr));
	                               
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

void * serverthread(void * parm)
{
   int tsd, tvisits, c;
   char     buf[100];           /* buffer for string the server sends */

   c = (int) parm;
   tsd = OmegaThread[c].sd;

   pthread_mutex_lock(&mut);
       tvisits = ++visits;
   pthread_mutex_unlock(&mut);

   sprintf(buf,"This server has been contacted %d time%s\n",tvisits, tvisits==1?".":"s.");

   printf("SERVER thread[%d]: %s", c,buf);
   send(tsd,buf,strlen(buf),0);

   sleep (2);   
   close(tsd);

   pthread_mutex_lock(&mut);
       OmegaThread[c].st=0;
   pthread_mutex_unlock(&mut);

   pthread_exit(0);
}    
