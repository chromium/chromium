#!/usr/bin/perl

print "Status: 200 OK\r\n";
print "Content-Type: application/x-javascript\r\n";
print "\r\n";
print "/* " . rand() . " */\r\n";
print "self.testToken = (self.testToken || 0) + 1;\r\n";