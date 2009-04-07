#include <stdio.h>
#include <string.h>

unsigned char buf[512]; 
unsigned char *buffer=&buf;
unsigned short *l;
unsigned char src=0xFF;
unsigned char dst=0x0;
unsigned char h[4];
unsigned char *hdr=&h;

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

void CreateTextMessage ( unsigned char src, unsigned char dst, char* message)
{
unsigned char len=strlen(message)+1;
unsigned short crc;
unsigned short n;
memcpy(buffer+7,message,len);
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
for(n=0; n<l; n++)
    printf("%02x",(unsigned char)buffer[n]);
printf ("\n");

}

int ParseBufferMessage ( unsigned char *src, unsigned char *dst, char* message)
{
unsigned short offset;
unsigned short len;
unsigned short crc;
if ( memcmp(hdr,buffer,4)==0 ) offset=0;
    else
    if ( memcmp(hdr,buffer+1,4)==0 ) offset=1;
	else return 1;
printf ("offset=%d\n",offset);
// Проверка CRC
src=buffer[offset+5];
dst=buffer[offset+4];
len=buffer[offset+6];
memcpy(&crc,buffer+offset+7+len,2);
if ( crc != SUM_CRC(offset+buffer+4,len+3) ) { printf ("CRC not match!\n"); return 2; }
memcpy(message,buffer+7,len);
printf ("SRC=%d,DST=%d, MSG=%s\n",src,dst, message);
}


int main ()
{
unsigned char st[]=" date?";
unsigned char reply[256];
unsigned char s;
unsigned char d;
hdr[0]=0x02;
hdr[1]=0x4d;
hdr[2]=0x6c;
hdr[3]=0x02;


CreateTextMessage(src,dst,st);
ParseBufferMessage(&s,&d,&reply);

return 0;
}
