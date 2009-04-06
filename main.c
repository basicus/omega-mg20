#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <iconv.h>
#include <errno.h>
#include "version.h"

#define SPEED B115200

/* Change to the serial port you want to use /dev/ttyUSB0, /dev/ttyS0, etc. */

#define PORT "/dev/ttyS0"
#define C_DST "dst="
#define BUF_SIZE 512



//char *buffer;

unsigned char buf[BUF_SIZE];
unsigned char *buffer=(unsigned char*)&buf;
const unsigned char h[]={0x02, 0x4d, 0x6c, 0x02};
unsigned char *hdr=(unsigned char*) &h;
pthread_mutex_t ser_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned short SUM_CRC (unsigned char *Address, unsigned char Lenght)
{
int N;
unsigned short WCRC=0;
for (N=1;N<=Lenght;N++,Address++)
{
    WCRC+=(*Address)^255;
    WCRC+=((*Address)*256);
}
return WCRC;
}

int CreateTextMessage ( unsigned char src, unsigned char dst, char* message,char* buffer)
{
unsigned char len=strlen(message)+1;
unsigned short crc;
unsigned short l;
char *send=malloc(len+1);
send[0]=0x20;
memcpy(send+1,message,len+1);
len = strlen(send)+1;
memcpy(buffer+7,send,len+1);
buffer[0]=0x02;
buffer[1]=0x4D;
buffer[2]=0x6C;
buffer[3]=0x02;
buffer[4]=dst;
buffer[5]=src;
buffer[6]=len;

crc=SUM_CRC(buffer+4,len+3);
memcpy(buffer+len+7,&crc,2);
l=len+9;
//for(n=0; n<l; n++)
//    printf("%02x",(unsigned char)buffer[n]);
//printf ("\n");
free (send); //???
return l;
}

int ParseBufferMessage ( unsigned char* src, unsigned char* dst,char *message,char *bfr, int l,int *n)
{
int offset; // найденное смещение от начала буффера
unsigned short len; // найденная длина текстового сообщения
unsigned short crc,crc2; // контрольная сумма
unsigned char flag=0;

char* msg; // указатель на предполагаемое начало сообщения
// Поиск заголовка в буффере
msg = (char*) memchr(bfr, *hdr,l);
while ( msg != NULL ) {
if ( memcmp(hdr,msg,4)!=0 ) { // не нашли заголовок в буффере
			    offset=&msg[0]-&bfr[0]; // находим смещение и ищем следующее вхождение
			    //printf ("Offset1 =%d, A1= %p; A2=%p\n",offset,msg,bfr);
			    msg = (char*) memchr(bfr+offset+1, *hdr,l-offset-1);
			    }
    else { flag=1;  break;}
}

if ( flag==0 ) return 1; // заголовок в буффере не найден

offset=&msg[0]-&bfr[0];
if (offset<l) {
//printf ("Offset2 = %d,A1= %p; A2=%p\n",offset,msg,bfr);
// Проверка CRC
*src= msg[5];
*dst= msg[4];
len= msg[6];
if ( (offset+len+9)<=l ) {
memcpy(&crc,msg+7+len,2);
crc2=SUM_CRC(msg+4,len+3);
if ( crc != crc2 ) { return 2; }
memcpy(message,msg+7,len+1);
//printf ("SRC=%d,DST=%d, MSG=%s\n",*src,*dst, message);
} else { return 1;}
}
else { return 1; }

*n = offset+len+9;
return 0;
/* Коды ошибок:
0 - все в порядке, в буффере найдено сообщение на указанном отрезке буффера
1 - в указанном отрезке не найдено заголовка, сообщение получено не полностью
2 - в указанном отрезке найден заголовок, но не верная контрольная сумма (вероятнее всего сообщение получено не до конца)
*/
}


void * ReadSerial (void *parm) {
char *reply=(char *) malloc(BUF_SIZE),*rreply; // для принятого сообщения
size_t sreply,sconv=2*BUF_SIZE,nconv;
char *conv=(char *)  malloc(2*BUF_SIZE),*rconv; // для перекодированного сообщения
//unsigned char* rcv;
//char *buffer=malloc(2*BUF_SIZE);

unsigned char s;
unsigned char d;
unsigned short r=0;
int fd=*(int *)parm;
int r1;
int n;
iconv_t win2utf;

win2utf = iconv_open ("UTF-8", "CP1251"); // осуществляем перекодировку из CP1251 в кодировку локали (UTF-8)

if (win2utf == (iconv_t) -1)
         { /* Что-то не так  */
           if (errno == EINVAL)
              printf ("Can't converse from '%s' to wchar_t not available\n","CP1251");
           else
            perror ("iconv_open");
            return -1; // М.б. надо выйти?
         }


while( 1 ) {
	pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
	r1 =  read( fd, buffer+r, 256 );
	pthread_mutex_unlock (&ser_mutex);

	if ( r1 > 0 ) {
	  r = r +r1;
	  //printf ("Readed some info %d:%d\n",r1,r);
	  //for(i=0; i<r1; i++) printf("%02x",(unsigned char)buffer[i]);
	  //printf ("\n");
	  while ( ParseBufferMessage(&s,&d,reply,buffer,r,&n)==0 ) { // корректно декодированное сообщение
	    r = r - n;
	    sreply=strlen(reply);
	    rreply=reply;
	    rconv=conv;
        sconv=2*BUF_SIZE;
	    // Кон

	    nconv = iconv (win2utf, &rreply, &sreply, &rconv, &sconv);
	    if ((nconv == (size_t) -1) & (errno == EINVAL))
             {
                /* TODO (bas#1#): Необходимо добавить обработку ошибок конвертации */                printf ("Error! Can't convert some input text\n");
             }
        *rconv='\0';
        printf ("SRC=%d DST=%d>%s\n",s,d,conv);
	    if ( r >0 ) {memcpy(buffer, buffer+n, r);} // копируем оставшееся сообщение в начало
            else {memset(buffer,0x00,n);}
        //memset(buffer+r,0x00,n);
        //memset(reply,0x00,BUF_SIZE);
	  }
    }
}


}

int main (int argc, char *argv[])
{
pthread_t thread_read;
char *input;
char *send=malloc(BUF_SIZE);
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


pthread_create(&thread_read,NULL, &ReadSerial, &fd);
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
l=CreateTextMessage (sr, ds, input, send);
pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
write(fd,send,l);
pthread_mutex_unlock (&ser_mutex);
}
free(input);
}
//free (input);
return 0;
}

