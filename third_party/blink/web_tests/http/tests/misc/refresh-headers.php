<?php
  $gotMaxAge=false;
  $gotNoCache=false;

  if (0 == strcasecmp($_SERVER["HTTP_CACHE_CONTROL"], "max-age=0"))
  {
    $gotMaxAge = true;
  }
    
  if ((0 == strcasecmp($_SERVER["HTTP_CACHE_CONTROL"], "no-cache")) ||
      (0 == strcasecmp($_SERVER["HTTP_PRAGMA"], "no-cache")))
  {
    $gotNoCache = true;
  }
  
  if ($gotNoCache) {
    echo '<p>Got a no-cache directive; FAILURE!</p>';
    echo '<script>if (window.testRunner) { testRunner.notifyDone(); }</script>';
  } else if ($gotMaxAge) {
    echo '<p>SUCCESS</p>';
    echo '<script>if (window.testRunner) { testRunner.notifyDone(); }</script>';
  } else {
    echo '<body onload="window.location.reload();">';
    echo '<p>No cache control headers, reloading...</p>';
    echo '<script>if (window.testRunner) { testRunner.waitUntilDone(); }</script>';
    echo '<script>function test() {window.location.reload();}</script>';
  }

  echo '<script>if (window.testRunner) { testRunner.dumpAsText(); }</script>';
  echo '<p>Test for <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5499">bug 5499</a>: Page reload does not send any cache control headers.</p>';
?>
