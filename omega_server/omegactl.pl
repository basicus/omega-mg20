#!/usr/bin/perl -w
# Omega console management client on the PERL
use strict;
use warnings;
use threads;
use threads::shared;
use IO::Socket;


my $sock = new IO::Socket::UNIX( Peer => '/tmp/omega.ctl',Type => SOCK_STREAM ) or die $!;

my $thr = threads->create(\&ReadConsole);

my $messg;
while () {
    $messg = <$sock>;
    if ( defined $messg ) {
        print STDOUT $messg;
        }
}


sub ReadConsole {
    my $line="none";
    while($line ne "quit"){
    println(":>");
    chomp($line=<STDIN>);
    if ($line ne "quit" ) {
			$sock->send("SEND $line\n");
			}
	else {		$sock->send("QUIT\n");
	}
    print STDOUT ("Sent!!!\n");
    }
    exit (0);
}

print ("Exiting...\n");

sub println{
   if ($_[0] eq ":>"){
         print STDOUT ":>";
        }
   else
      {
        print STDOUT "$_[0]\n";
   }
}
                           