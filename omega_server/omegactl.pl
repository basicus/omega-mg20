#!/usr/bin/perl -w
use strict;

# Omega client on the PERL

use Socket;
use IO::Handle;

socket(TSOCK, PF_UNIX, SOCK_STREAM,0);
connect(TSOCK, sockaddr_un("/tmp/omega.ctl")) or print("ERROR!");

print TSOCK "Hello!";
print TSOCK "quit";

#while (defined(my $messg = <TSOCK>)) {
#        print $messg;
#	        print TSOCK "Hello server!";
TSOCK->flush;
#			}