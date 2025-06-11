<?php
header("Link: <   http://wut.com.test/>; rel=dns-prefetch");
?>
<!DOCTYPE html>
<script>
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
</script>
This test check if a Link header triggered a dns prefetch.
<script>
    if (window.testRunner)
        testRunner.notifyDone();
</script>
