#!/usr/bin/perl -w

exit (main (@ARGV));

sub uniq
{
    my (@a) = sort (@_);
    my @u = ();

    push (@u, shift (@a));
    foreach my $e (@a) {
        if ($u[$#u] ne $e) {
            push (@u, $e);
        } 
    }
    return @u;
}

sub stripped
{
    my ($args) = @_;
    my $ret = "";

    foreach my $p (split (',', $args)) {
        $p =~ s/^.*\s+//;
        $p =~ s/^\*+//;
        $p =~ s/\[\s*\]//g;
        $ret .= "$p, ";
    }
    $ret =~ s/,\s+$//;

    if ($ret eq "void") {
        $ret = "";
    }

    return $ret;
}

sub main {
    my ($f) = @_;
    open (my $fp, $f) || die "unable to open ${f}: $!";
    my @defines = ();
    my @includes = ();
    my @protos = ();
    my @functions = ();
    my @init = ();
    my @ptrs = ();

    push (@defines, "#define _GNU_SOURCE\n");

    push (@includes, "#include <stdio.h>\n");
    push (@includes, "#include <dlfcn.h>\n");
    push (@includes, "#include <stdlib.h>\n");
    
    while (my $l = readline ($fp)) {
        chomp ($l);

        $l =~ s/\/\/.+$//;
        $l =~ s/^\s*//;
        $l =~ s/\s*$//;

        if ($l eq "") {
            next;
        }
        if ($l =~ /^\s*(\S+\s*\**)\s*(\S+)\s*\((.+)\)\s*;/) {
            my ($ret, $fun, $args) = ($1, $2, $3);
            $ret =~ s/\s+$//;
            push (@protos, sprintf ("static %s (* gp_%s) (%s) = NULL;\n", $ret, $fun, $args));
            push (@functions, sprintf("%s %s (%s)\n\{\n", $ret, $fun, $args));
            push (@ptrs, "gp_${fun}");
            if ($ret ne "void") {
                push (@functions, sprintf("  return gp_%s (%s);\n}\n\n", $fun, stripped ($args)));
            }
            else {
                push (@functions, sprintf("  gp_%s (%s);\n}\n\n", $fun, $args));
            }
            push (@init, sprintf ("  gp_%s = (%s (*)(%s)) safe_dlsym (RTLD_NEXT, \"%s\");\n", $fun, $ret, $args, $fun));
        }
        elsif ($l =~ /(\#define\s*.+)/)
        {
            push (@defines, "${l}\n");
        }
        elsif ($l =~ /(\#include\s*.+)/)
        {
            push (@includes, "${l}\n");
        }
        else {
            printf ("ERROR: [%s]\n", $l);
        }
    }
    
    close ($fp);
    printf ("// Compile with gcc -o <obj> <file> -ldl to make an object\n");
    printf ("// Compile with gcc -o <bin> -DTEST_DLSYM <file> -ldl to test\n");
    printf ("\n");
    print uniq @defines;
    printf ("\n\n");
    print uniq @includes;
    printf ("\n\n");
    print @protos;
    printf ("\n\n");

    printf ("static void *safe_dlsym (void *handle, const char *symbol);\n");
    printf ("void *safe_dlsym (void *handle, const char *symbol)\n");
    printf ("{\n");
    printf ("  void *r;\n");
    printf ("\n");
    printf ("  r = dlsym (handle, symbol);\n");
    printf ("  if (r == NULL)\n");
    printf ("  {\n");
    printf ("    fprintf (stderr, \"Couldn't get an address for \\\"%%s ()\\\"\\n\", symbol);\n");
    printf ("  }\n");
    printf ("  return r;\n");
    printf ("}\n");
    printf ("\n");

    print @functions;
    printf ("void static init (void) __attribute__((constructor)); // initialize this library\n");
    printf ("static void init (void)\n{\n");
    print @init;
    printf ("\n");
    printf("  if (%s == NULL ||\n", shift (@ptrs));
    my $tmp = pop (@ptrs);
    foreach my $p (@ptrs)
    {
        printf ("      %s == NULL ||\n", $p);
    }
    printf("      %s == NULL)\n", $tmp);
    printf ("  {\n");
    printf ("    fprintf (stderr, \"\\n\");\n");
    printf ("    fprintf (stderr, \"failed to get all symbols - refer to stderr for failure(s)\\n\");\n");
    printf ("    fprintf (stderr, \"exiting\\n\");\n");
    printf ("    exit (1);\n");
    printf ("  }\n");
    printf ("}\n");
    printf ("\n");
    printf ("#ifdef TEST_DLSYM\n");
    printf ("int main (int argc, char **argv)\n");
    printf ("{\n");
    printf ("  printf (\"This appears to pass - you can modify the functions to capture additional data and link with the object.\\n\");\n");
    printf ("  return 0;\n");
    printf ("}\n");
    printf ("#endif //TEST_DLYSM\n");
    return 0;
}
