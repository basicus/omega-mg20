#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>



const unsigned char mg20_h[]={0x02, 0x4d, 0x6c, 0x02};
unsigned char *mg20_hdr=(unsigned char*) &mg20_h;


/* Функция вычисления контрольной суммы */
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

#ifdef SERIAL
pthread_mutex_t ser_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Часть Serial */
/* Функция для формирования текстового сообщения */
int CreateTextMessage (unsigned char src, unsigned char dst, char* message,char* buffer)
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
#ifdef DEBUG
    int n;
    for(n=0; n<l; n++) printf("%02x",(unsigned char)buffer[n]);
    printf ("\n");
#endif
free (send);

return l;
}

/* Парсинг текстового сообщения из буффера */
int ParseBufferMessage ( unsigned char* src, unsigned char* dst,char *message,char *bfr, int l,int *n)
{
int offset; // найденное смещение от начала буффера
unsigned short len; // найденная длина текстового сообщения
unsigned short crc,crc2; // контрольная сумма
unsigned char flag=0;

char* msg; // указатель на предполагаемое начало сообщения
// Поиск заголовка в буффере
msg = (char*) memchr(bfr, *mg20_hdr,l);
while ( msg != NULL ) {
if ( memcmp(mg20_hdr,msg,4)!=0 ) { // не нашли заголовок в буффере
			    offset=&msg[0]-&bfr[0]; // находим смещение и ищем следующее вхождение
			    //printf ("Offset1 =%d, A1= %p; A2=%p\n",offset,msg,bfr);
			    msg = (char*) memchr(bfr+offset+1, *mg20_hdr,l-offset-1);
			    }
    else { flag=1;  break;}
}

if ( flag==0 ) return 1; // заголовок в буффере не найден

offset=&msg[0]-&bfr[0];
if (offset<l) {
    #ifdef DEBUG
        printf ("Offset2 = %d,A1= %p; A2=%p\n",offset,msg,bfr);
    #endif

    /* Проверка CRC */
    *src= msg[5];
    *dst= msg[4];
    len= msg[6];
    if ( (offset+len+9)<=l ) {
        memcpy(&crc,msg+7+len,2);
        crc2=SUM_CRC(msg+4,len+3);
        if ( crc != crc2 ) { return 2; }
        memcpy(message,msg+7,len+1);
        #ifdef DEBUG
            printf ("SRC=%d,DST=%d, MSG=%s\n",*src,*dst, message);
        #endif
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

/* Функция чтения информации из дескриптора файла и вывод на экран принятого сообщения */
void * OmegaReadSerial (void *parm)
{
char *reply=(char *) malloc(BUF_SIZE),*rreply; // для принятого сообщения
size_t sreply,sconv=2*BUF_SIZE,nconv;
char *conv=(char *)  malloc(2*BUF_SIZE),*rconv; // для перекодированного сообщения
unsigned char buf[BUF_SIZE];
unsigned char *buffer=(unsigned char*)&buf;
unsigned char s;
unsigned char d;
unsigned short r=0;
int fd=*(int *)parm;
int r1;
int n;
iconv_t win2utf;
time_t rawtime;
struct tm * timeinfo;
char *ctime=(char *) malloc(128);

win2utf = iconv_open ("UTF-8", "CP1251"); // осуществляем перекодировку из CP1251 в кодировку локали (UTF-8)
if (win2utf == (iconv_t) -1)
         { printf ("Can't converse from '%s' to wchar_t not available\n","CP1251"); }

/* бесконечный цикл */
while( 1 ) {
	pthread_mutex_lock (&ser_mutex); // организуем блокировку на время чтения
	r1 =  read( fd, buffer+r, 256 );
	pthread_mutex_unlock (&ser_mutex);

	if ( r1 > 0 ) {
	  r = r +r1;
	  #ifdef DEBUG
	  int i;
        printf ("Readed some info %d:%d\n",r1,r);
        for(i=0; i<r1; i++) printf("%02x",(unsigned char)buffer[i]);
        printf ("\n");
	  #endif
	  while ( ParseBufferMessage(&s,&d,reply,buffer,r,&n)==0 ) { // корректно декодированное сообщение
	    r = r - n;
	    sreply=strlen(reply);
	    rreply=reply;
	    rconv=conv;
        sconv=2*BUF_SIZE;

	    nconv = iconv (win2utf, &rreply, &sreply, &rconv, &sconv);
	    if ((nconv == (size_t) -1) & (errno == EINVAL))
             {
                /* TODO (bas#1#): Необходимо добавить обработку ошибок конвертации */
                printf ("Error! Can't convert some input text\n");
             }
        *rconv='\0';
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        strftime (ctime,128,"%d.%m.%Y %H:%M:%S",timeinfo);

        printf ("%s <SRC=%d DST=%d>%s\n",ctime,s,d,conv);
	    if ( r >0 ) {memcpy(buffer, buffer+n, r);} // копируем оставшееся сообщение в начало
            else {memset(buffer,0x00,n);}
	  }
    }
}
}
#endif

#ifdef NET
/* Часть Network */
/* Функция для формирования текстового сообщения */
int CreateTextMessageNet (unsigned short src, unsigned short dst, char* message,char* buffer)
{
unsigned char len=strlen(message)+1;
unsigned short crc;
unsigned short l;
char *send=malloc(len+1);
send[0]=0x20;
memcpy(send+1,message,len+1);
len = strlen(send)+1;
memcpy(buffer+9,send,len+1);
buffer[0]=0x02;
buffer[1]=0x4D;
buffer[2]=0x6C;
buffer[3]=0x02;
buffer[4]=dst;
buffer[6]=src;
buffer[8]=len;

crc=SUM_CRC(buffer+4,len+5);
memcpy(buffer+len+9,&crc,2);
l=len+11;
#ifdef DEBUG
    int n;
    for(n=0; n<l; n++) printf("%02x",(unsigned char)buffer[n]);
    printf ("\n");
#endif
free (send);

return l;
}

/* Парсинг текстового сообщения из буффера Network */
int ParseBufferMessageNet ( unsigned short* src, unsigned short* dst,char *message,char *bfr, int l,int *n)
{
int offset; // найденное смещение от начала буффера
unsigned short len; // найденная длина текстового сообщения
unsigned short crc,crc2; // контрольная сумма
unsigned char flag=0;

char* msg; // указатель на предполагаемое начало сообщения
// Поиск заголовка в буффере
msg = (char*) memchr(bfr, *mg20_hdr,l);
while ( msg != NULL ) {
if ( memcmp(mg20_hdr,msg,4)!=0 ) { // не нашли заголовок в буффере
			    offset=&msg[0]-&bfr[0]; // находим смещение и ищем следующее вхождение
			    //printf ("Offset1 =%d, A1= %p; A2=%p\n",offset,msg,bfr);
			    msg = (char*) memchr(bfr+offset+1, *mg20_hdr,l-offset-1);
			    }
    else { flag=1;  break;}
}

if ( flag==0 ) return 1; // заголовок в буффере не найден

offset=&msg[0]-&bfr[0];
if (offset<l) {
    #ifdef DEBUG
        printf ("Offset2 = %d,A1= %p; A2=%p\n",offset,msg,bfr);
    #endif

    /* Проверка CRC */
    *src= msg[6];
    *dst= msg[4];
    len= msg[8];
    if ( (offset+len+11)<=l ) {
        memcpy(&crc,msg+9+len,2);
        crc2=SUM_CRC(msg+4,len+5);
        if ( crc != crc2 ) { return 2; }
        memcpy(message,msg+9,len+1);
        #ifdef DEBUG
            printf ("SRC=%d,DST=%d, MSG=%s\n",*src,*dst, message);
        #endif
    } else { return 1;}
    }
else { return 1; }

*n = offset+len+11;
return 0;
/* Коды ошибок:
0 - все в порядке, в буффере найдено сообщение на указанном отрезке буффера
1 - в указанном отрезке не найдено заголовка, сообщение получено не полностью
2 - в указанном отрезке найден заголовок, но не верная контрольная сумма (вероятнее всего сообщение получено не до конца)
*/
}

void NetPassword (unsigned short src, unsigned short dst, char *net, char * pass) {
    char buf[100];
    char msg[100];
    int n, r;
    sprintf(msg,"%s %s",net,pass);

    printf ("%s",msg);
    r = CreateTextMessageNet(src,dst,msg,buf);
    printf ("\n");
    printf("Coded auth:");
    for(n=0; n<r; n++) printf("%02x",(unsigned char)buf[n]);
    ParseBufferMessageNet(&src,&dst,&msg,&buf,r,&n);
    printf ("\n%s\n",msg);
}

int NetAuth (char *net, char * pass,char *auth) {
    char msg[100];
    int n, r;
    sprintf(msg," %s-%s",net,pass);
    if ( strcmp(msg,auth)==0 ) return 1; /* auth passed */
        else return 0;
}

#endif
