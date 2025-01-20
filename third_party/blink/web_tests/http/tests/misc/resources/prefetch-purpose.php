<?php
header("Cache-Control: max-age=3600");
echo "Purpose: " . $_SERVER["HTTP_PURPOSE"];
?>

<script>
testRunner.notifyDone();
</script>

<p>This test verifies that prefetches are sent with the HTTP request
header <b>Purpose: prefetch</b>.  To do this, the root page has a
prefetch link targetting this subresource which contains a PHP script
(resources/prefetch-purpose.php).  The PHP prints the value of the
Purpose header into the document.  Later, the root page sets
window.location to target this script, which should have "Purpose:
prefetch" in its output if it's served from cache.
