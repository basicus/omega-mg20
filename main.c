#define BUF_SIZE 512

#include <fcntl.h>
#include <termios.h>
#include "version.h"
#include <time.h>
#include "mg20func.h"

#define DEBUG 1
#define SERIAL 1
#define SPEED B115200
#define PORT "/dev/ttyS0"
#define C_DST "dst="
#define NET 2

int main (int argc, char *argv[])
{
pthread_t thread_read;
char *input, *input_b;
char *send=malloc(2*BUF_SIZE);

char *conv=(char *) malloc(BUF_SIZE),*sconv; // для переданного сообщения
size_t lsend,lsend_b=2*BUF_SIZE,nconv;
iconv_t utf2win;
unsigned char sr=254;
unsigned char ds=0x00;
int l,ds_n;

struct termios options;
int fd;
char *port;
char *parse;
/*
Если количество аргументов равно нулю, то мы запускаемся со стандартными параметрами
Если количество аргументов равно двум, то мы запускаемся с параметрами пользователя -
1-й параметр - порт, 2-й параметр адрес
*/

if (argc == 3) {
         sr = atoi(argv[2]);
         port = argv[1];
     }
     else
     {
         port = PORT;
     }

// Работа с последовательным портом
fd = open( port, O_RDWR | O_NOCTTY | O_NDELAY);
if (fd <0) {perror(argv[1]); exit(-1); }
bzero(&options, sizeof(options));
options.c_cflag = SPEED | CS8 | CLOCAL | CREAD | IGNPAR;
options.c_cc[VTIME]    = 50;
options.c_cc[VMIN]     = 0;     /* read by char */

tcflush(fd, TCIFLUSH);
tcsetattr(fd, TCSANOW, &options);

utf2win = iconv_open ("CP1251","UTF-8"); // осуществляем перекодировку из UTF-8 в кодировку Omega (CP1251)

if (utf2win == (iconv_t) -1)
         { /* Что-то не так  */
           if (errno == EINVAL)
              printf ("Can't converse from '%s' to CP1251 not available\n","UTF-8");
              exit (1); // Ошибка
           }



pthread_create(&thread_read,NULL, &OmegaReadSerial, &fd);
printf ("OMEGA-MG20 serial comman line tool. Version %d.%d%s (c) Sergey Butenin.\nUsing port=%s, src address=%d, destination=%d\n",(int) MAJOR,(int) MINOR,STATUS_SHORT,port,sr,ds);
printf ("Please use commands:\nquit - to exit;\ndst=(0..255) - to set new destination address\nother - to send text message to OMEGA\n");
while (1) {
parse = NULL;
input = (char *) readline ("# ");
if ( strcmp (input,"quit") ==0 ) break;
parse = strstr( input,"dst=");
if( parse != NULL ) {
    ds_n = atoi (parse+strlen(C_DST));
    if ( ds_n >0 && ds_n<256 ) {
        ds=ds_n;
        printf ("New destination address=%i\n",ds);
    }
} else if ( strlen (input) !=0 ) {
        lsend=strlen(input);
	    input_b=input;
	    sconv=conv;
        lsend_b=2*BUF_SIZE;
	    // Кон

	    nconv = iconv (utf2win, &input_b, &lsend, &sconv, &lsend_b);
	    if ((nconv == (size_t) -1) & (errno == EINVAL))
             {
                /* TODO (bas#1#): Необходимо добавить обработку ошибок конвертации */
                printf ("Error! Can't convert some input text\n");
             }
        *sconv='\0';

        l=CreateTextMessage (sr, ds, conv, send);
        pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
        write(fd,send,l);
        pthread_mutex_unlock (&ser_mutex);
}
free(input);
}
return 0;
}

