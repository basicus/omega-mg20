/* To compile me in Solaris type:  gcc -o client client.c -lsocket -lnsl */
/* To compile me in Linux type:  gcc -o client client.c */

/* client.c - code for example client that uses TCP         */
/* From Computer Networks and Internets by Douglas F. Comer */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>

#define closesocket      close
#define PROTOPORT        7701        /* default protocol port number */

extern int               errno;
char   localhost[] = "localhost";    /* default host name            */
/*---------------------------------------------------------------------
 * Program:   client
 *
 * Purpose:   allocate a socket, connect to a server, and print all output
 *
 * Syntax:    client [ host [port] ]
 *
 *              host - name of a computer on which server is executing
 *              port - protocol port number server is using
 *
 * Note:      Both arguments are optional.  If no host name is specified,
 *            the client uses "localhost";  if no protocol port is
 *            specified, the client uses the default given by PROTOPORT.
 *
 *---------------------------------------------------------------------
 */
main(int argc, char *argv[])
{
   struct  hostent  *ptrh;   /* pointer to a host table entry       */
   struct  protoent *ptrp;   /* point to a protocol table entry     */
   struct  sockaddr_in sad;  /* structure to hold server's address  */
   int     sd;               /* socket descriptor                   */
   int     port;             /* protocol port number                */
   char    *host;            /* pointer to host name                */
   int     n;                /* number of characters read           */
   char    buf[1000];        /* buffer for data from the server     */

   char   auth[] = {0x02, 0x4d, 0x6c, 0x02, 0xfe, 0xff, 0x98, 0x08, 0x0e, 0x20, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x20,0x63,0x65, 0x72, 0x6e, 0x00, 0x75, 0x83};
   memset((char *)&sad,0,sizeof(sad));  /* clear sockaddr structure */
   sad.sin_family = AF_INET;            /* set family to Internet   */

   /* Check command-line argument for protocol port and extract     */
   /* port number if on is specified.  Otherwise, use the default   */
   /* port value biven by constant PROTOPORT                        */

   if (argc > 2) port = atoi(argv[2]);
   else port = PROTOPORT;
   
   if (port > 0) sad.sin_port = htons((u_short)port);
   else 
     { fprintf( stderr,"bad port number %s\n", argv[2]);
          exit(1);
     }
   
   //auth[5] = (unsigned char) random(255);
   if (argc > 1 ) host = argv[1];
   else host = localhost;

   ptrh = gethostbyname(host);
   if( ((char *)ptrh) == NULL)
     { fprintf( stderr, "invalid host:  %s\n", host);
       exit(1);
     }
   
   memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

   if ( ((int)(ptrp = getprotobyname("tcp"))) == 0)
     { fprintf( stderr, "cannot map \"tcp\" to protocol number\n");
       exit(1);
     }

   sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
   if (sd < 0)
     { fprintf( stderr, "socket creation failed\n");
       exit(1);
     }

   if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0)
     { fprintf( stderr, "connect failed\n");
       exit(1);
     }
	
	/* auth */
	write(sd,auth,25);
	sleep (1);
	write(sd,auth,25); 


   n = recv(sd, buf, sizeof(buf), 0);
/*   while(n > 0)
     { 
       buf[n] = '\0';
       fprintf( stderr, "CLIENT: %s", buf);
       n = recv(sd, buf, sizeof(buf), 0);
     }
*/
	sleep(10);

   closesocket(sd);
   exit(0);
}



