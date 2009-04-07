#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>



/* Change to the baud rate of the port B2400, B9600, B19200, etc */
#define SPEED B115200

/* Change to the serial port you want to use /dev/ttyUSB0, /dev/ttyS0, etc. */

#define PORT "/dev/ttyS0"

unsigned char buf[1024]; 
unsigned char *buffer=&buf;
const unsigned char h[]={0x02, 0x4d, 0x6c, 0x02};
unsigned char *hdr=&h;
pthread_mutex_t ser_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned short SUM_CRC ( unsigned char *Address, unsigned char Lenght)
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

int CreateTextMessage ( unsigned char src, unsigned char dst, char* message, char* buffer)
{
unsigned char len=strlen(message)+1;
unsigned short crc;
unsigned short n;
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
return l;
}

int ParseBufferMessage ( unsigned char* src, unsigned char* dst, char* message,char* bfr, int l)
{
int offset; // найденное смещение от начала буффера 
unsigned short len; // найденная длина текстового сообщения
unsigned short crc; // контрольная сумма
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
//printf ("Offset2 = %d,A1= %p; A2=%p\n",offset,msg,bfr);
// Проверка CRC
*src= msg[5];
*dst= msg[4];
len= msg[6];
memcpy(&crc,msg+7+len,2);
if ( crc != SUM_CRC(msg+4,len+3) ) { return 2; }
memcpy(message,msg+7,len+1);
//printf ("SRC=%d,DST=%d, MSG=%s\n",*src,*dst, message);
// Изменяем значение указателя буффера на указатель конца сообщения

return offset+len+9;
/* Коды ошибок:
0 - все в порядке, в буффере найдено сообщение на указанном отрезке буффера
1 - в указанном отрезке не найдено заголовка
2 - в указанном отрезке найден заголовок, но не верная контрольная сумма (вероятнее всего сообщение получено не до конца)
*/
}


void * ReadSerial (void *parm) {
unsigned char reply[256];
unsigned char* rcv;
unsigned char s;
unsigned char d;
unsigned short r=0;
int fd=*(int *)parm;
int r1;
unsigned short n,i;


while( 1 ) {
	pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
	r1 =  read( fd, buffer+r, 256 );
	pthread_mutex_unlock (&ser_mutex);
	
	if ( r1 > 0 ) {
	  r = r +r1;
	  //printf ("Readed some info %d:%d\n",r1,r);
	  //for(i=0; i<r1; i++) printf("%02x",(unsigned char)buffer[i]);
	  //printf ("\n");
	  n  =  ParseBufferMessage(&s,&d,&reply,buffer,r);
	  if ( n > 2 ) {
	    r = r - n;
	    //printf ("New r=%d, n=%d, src=%d; dst=%d\n",r,n,s,d);
	    printf ("\nSRC=%d DST=%d>%s",s,d,reply);
	    if ( r >0 ) memcpy(buffer, buffer+n, r); // копируем оставшееся сообщение в начало   
		else memset(buffer,0x00,n);
	  }
    }
}

}

int main ()
{
unsigned char st[]=" date?";
pthread_t thread_read;
char *input;
char *send=malloc(512);
unsigned char sr=254;
unsigned char ds=0x00;
int l;

// Работа с последовательным портом
int fd = open( PORT, O_RDWR | O_NOCTTY | O_NDELAY);
if (fd <0) {perror(PORT); exit(-1); }
struct termios options;
bzero(&options, sizeof(options));
options.c_cflag = SPEED | CS8 | CLOCAL | CREAD | IGNPAR;
options.c_cc[VTIME]    = 50;    
options.c_cc[VMIN]     = 0;     /* read by char */
	    
tcflush(fd, TCIFLUSH);
tcsetattr(fd, TCSANOW, &options);




//CreateTextMessage(src,dst,st);
pthread_create(&thread_read,NULL, &ReadSerial, &fd);
printf ("Please enter commands for OMEGA-MG20 \(type \`quit\` to exit\)\n");
while (1) { 
input = readline ("# ");
if ( strcmp (input,"quit") ==0 ) break;
if ( strlen (input) !=0 ) {
//printf ("You entered=%s\n",input);
l=CreateTextMessage ( sr, ds, input, send);
pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
write(fd,send,l);
pthread_mutex_unlock (&ser_mutex);
}
free (input);
}
free (input);
return 0;
}

