#!/usr/bin/perl -w

exit (main (@ARGV));

sub main
{
    my ($user, $host, $port, @additional) = @_;

    if (   (! defined $user)
        || (! defined $host)
        || (! defined $port) )
    {
        printf ("Specify a username, a host name, and a port #\n");
        return 1;
    }
    else
    {
        my $hostPort = 0;
        if ($host =~ /(\S+):(\d+)/)
        {
            $host = $1;
            $hostPort = $2;
        }
        
        my $echoPort =    32000 + $port;
        my $sslPort  =     3000 + $port;
        my $httpPort =     8000 + $port;
        my $sleep    = 31540000 + $port;

        my $cmd = "";

        $cmd .= "autossh -f";
        #$cmd .= " -M ${echoPort}";

        $cmd .= " -M 0";
        $cmd .= " -o \"ExitOnForwardFailure=yes\"";
        $cmd .= " -o \"ServerAliveInterval 30\"";
        $cmd .= " -o \"ServerAliveCountMax 3\"";

        $cmd .= " -R ${sslPort}:127.0.0.1:22";
        $cmd .= " -R ${httpPort}:127.0.0.1:80";
        $cmd .= ' ' . join (' ', @additional);
        if ($hostPort != 0)
        {
            $cmd .= " -p ${hostPort}";
        }
        $cmd .= " ${user}\@${host}";
        $cmd .= " sleep ${sleep}";

        $cmd =~ s/\*/\\*/;
        
        if ($host eq "test")
        {
            printf ("%s\n", $cmd);
        }
        else
        {
            system ($cmd);
        }
    }

    
    return 0;
}
