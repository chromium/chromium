#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

my $resourceType = $cgi->param("type");
my $wait = $cgi->param("wait");

if ($wait) {
    sleep(2);
}

if ($resourceType eq "script") {
    print "Location: http://127.0.0.1:8000/security/contentSecurityPolicy/resources/script-redirect-not-allowed.js";
}

if ($resourceType eq "image") {
    print "Location: http://127.0.0.1:8000/resources/square20.jpg";
}

if ($resourceType eq "frame") {
    print "Location: http://127.0.0.1:8000/security/contentSecurityPolicy/resources/iframe-redirect-not-allowed.html";
}

if ($resourceType eq "stylesheet") {
    print "Location: http://127.0.0.1:8000/security/contentSecurityPolicy/resources/stylesheet-redirect-not-allowed.css";
}

if ($resourceType eq "xhr") {
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n";
    print "Location: http://127.0.0.1:8000/security/contentSecurityPolicy/resources/xhr-redirect-not-allowed.pl";
}

print "\r\n\r\n";
