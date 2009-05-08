#!/usr/bin/perl -w
use strict;
use Switch;

# Omega client on the PERL

use Socket;
use IO::Handle;

socket(TSOCK, PF_UNIX, SOCK_STREAM,0);
connect(TSOCK, sockaddr_un("/tmp/omega.ctl")) or print("ERROR!\n");

if (scalar(@ARGV)==0) {print ("Error: Not enough parameters.\n"); exit (1);}

switch($ARGV[0]){
case "send" { print TSOCK "SEND $ARGV[1] $ARGV[2]\n"; }
case "version" {print TSOCK "VERSION\n";}
case "status" {print TSOCK "STATUS\n"; }
else {print "Usage: omegactl_cmd.pl <SEND|VERSION|STATUS> [ADDRESS] [MSG]";}
	  }


#print TSOCK "send $ARGV[0] $ARGV[0]\n";
#print TSOCK "quit1\n";
#print TSOCK "quit\n";
#print TSOCK "status\n";
#while (defined(my $messg = <TSOCK>)) {
#        print $messg;
#	        print TSOCK "Hello server!";

