#!/usr/bin/perl -wT

use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html\n";
print "\n";

print <<"END";
<form method="POST" enctype="application/x-www-form-urlencoded" action="#foo">
  <input type="text" name="f">
  <input type="submit" value="Submit">
</form>
<div id="result"></div>
<script src="/resources/prevent-bfcache.js"></script>
<script>
onload = function() {
  setTimeout(async function () {
    await preventBFCache();
  }, 0);
  alert("stage: " + sessionStorage.stage);
  switch (sessionStorage.stage++) {
  case 1:
    // Submit form in a timeout to make sure that we create a new back/forward list item.
    setTimeout(function() {document.forms[0].submit();}, 0);
    break;
  case 2:
    history.back();
    break;
  case 3:
    document.getElementById("result").innerText = "PASS";
    if (window.testRunner)
      testRunner.notifyDone();
    break;
  }
}
</script>
END
