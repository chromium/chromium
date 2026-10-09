#!/usr/bin/perl
# Script to generate a 304 HTTP status with (illegal) X-Frame-Options headers.
# Relies on its nph- filename to invoke the CGI non-parsed-header facility.
#
# Because this is an nph- script, Apache sends our output to the client
# verbatim. On Windows, Perl opens STDOUT in text mode and would rewrite each
# "\r\n" below as "\r\r\n", which Chromium does not accept as a header line
# terminator. Force binary mode so the bytes we print are the bytes on the
# wire on every platform.
binmode(STDOUT);

$protocol = $ENV{'SERVER_PROTOCOL'};
$software = $ENV{'SERVER_SOFTWARE'};
$if_modified = $ENV{'HTTP_IF_MODIFIED_SINCE'};

if ($if_modified) {
    print "$protocol 304 Not Modified\r\n";
    print "Server: $software\r\n";
    print "X-Frame-Options: deny\r\n";
    print "Connection: close\r\n";
    print "\r\n";
    exit(0);
}

print "$protocol 200 OK\r\n";
print "Server: $software\r\n";
print "Content-type: text/html\r\n";
print "Expires: Wed, 21 Nov 2037 19:19:13 +0000\r\n";
print "Last-Modified: Mon, 08 Nov 2010 19:19:13 +0000\r\n";
print "X-Frame-Options: allowall\r\n";
print "\r\n";
print "<!DOCTYPE html>\r\n";
print "<html>\r\n";
print "<body>\r\n";
print "<script>alert(\"This must fire twice\");</script>\r\n";
print "</body>\r\n";
print "</html>\r\n";
