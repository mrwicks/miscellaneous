#!/usr/bin/perl -w

use Env;
use CGI;
use File::Temp qw/ tempfile tempdir /;

exit (main (@ARGV));

sub isCgi
{
    my $ret = 0;
    if ( defined $ENV{"HTTP_ACCEPT"} ||
         defined $ENV{"HTTP_ACCEPT_ENCODING"} ||
         defined $ENV{"HTTP_ACCEPT_LANGUAGE"} ||
         defined $ENV{"HTTP_CONNECTION"} ||
         defined $ENV{"HTTP_HOST"} ||
         defined $ENV{"HTTP_REFERER"} ||
         defined $ENV{"HTTP_HOST"} )
    {
        $ret = 1;
    }

    return $ret;
}

sub printCgiForm
{
    my ($cgi) = @_;

    my $program = $0;
    $program =~ s/^.+\///g;

    print "Enter a pcap file to decode:<p>\n";
    print $cgi->start_form(-method=>"post",
                           -action=>$program,
                           -enctype=>"multipart/form-data");
    print "\n";
    print $cgi->filefield(-name=>'uploaded_file',
                          -default=>'starting value',
                          -size=>50,
                          -maxlength=>255);
    print "\n";
    print $cgi->submit(-name=>'upload_button',
                       -value=>'upload');
    print $cgi->end_form;    
}

sub doUploadCgiScript
{
    my ($cgi) = @_;
    my $fpIn;
    my $fileName;
    my $tmpFileName;
    
    print $cgi->header; # prints Content-Type:
    $fileName = $cgi->param ("uploaded_file");
    $fpIn = $cgi->upload ("uploaded_file");

    if (! defined ($fileName) || $fileName == "")
    {
        print $cgi->start_html(-title => 'PCAP->(Simplified)XML decoding form');
        printCgiForm ($cgi);
        print $cgi->end_html;

        # don't return because this page will be called again when data is supplied
        exit (0);
    }
    else
    {
        # actually upload
        my $buffer;
        my $totalBytes;
        my $bytesRead;
        my $fpOut;

        ($fpOut, $tmpFileName) = tempfile ();

        binmode ($fpIn);
        binmode ($fpOut);
        while ($bytesRead = read ($fpIn, $buffer, 1024))
        {
            print $fpOut $buffer;
            $totalBytes += $bytesRead;
        }
        close ($fpIn);
        close ($fpOut);
    }

    return ($fileName, $tmpFileName);
}

sub bail
{
    my ($cgi, $line) = @_;
    print $cgi->start_html(-title => "bail");
    if (1)
    {
        print "<pre>\n";
        print "${line}\n";
        foreach my $k (sort keys %ENV)
        {
            print "$k : $ENV{$k}\n";
        }
        print "</pre>\n";
    }
    print $cgi->end_form;
    exit (0);
}

sub main
{
    my ($fileName) = @_;
    my $tmpFileName;
    my $cgi = new CGI ();
    my $fh;
    
    if (isCgi ())
    {
        ($fileName, $tmpFileName) = doUploadCgiScript ($cgi);

        # uploaded file is in $tmpFileName, the original name is in $fileName
    }

    if (defined ($fileName))
    {
        # this does whatever the CGI script does, you can invoke it on the command
        # line with the argument as the file uploaded
        print $cgi->start_html(-title => "Decoded pcap file: ${fileName}");
        print "filename = ${fileName}<br>\n";
        if (defined $tmpFileName)
        {
            print "temp filename = ${tmpFileName}<br>\n";
        }
        print $cgi->end_form;
    }
    else
    {
        printf ("When running from the command line, supply a file to \"upload\"\n");
    }
    return 0;
}
