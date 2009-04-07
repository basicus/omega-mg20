#include <fcntl.h>
#include <stdio.h>
#include <termios.h>

#include <stdlib.h>
#include <strings.h>

/* Change to the baud rate of the port B2400, B9600, B19200, etc */
#define SPEED B115200

/* Change to the serial port you want to use /dev/ttyUSB0, /dev/ttyS0, etc. */

#define PORT "/dev/ttyS0"

int main( ){
    int fd = open( PORT, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd <0) {perror(PORT); exit(-1); }
    struct termios options;

    bzero(&options, sizeof(options));

    options.c_cflag = SPEED | CS8 | CLOCAL | CREAD | IGNPAR;
    options.c_cc[VTIME]    = 10;    
    options.c_cc[VMIN]     = 0;     /* read by char */
	    
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    int r;
    unsigned char buf[512];
    while( 1 ){
        r = read( fd, buf, 512 );
	if ( r >0 ) {
        printf ("Received %d bytes\n",r);
	}
    }
}
