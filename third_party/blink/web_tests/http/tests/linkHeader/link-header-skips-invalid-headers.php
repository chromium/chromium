<?php
header("Link: <http://foo.com/>; rel=preconnect; anchor=\"foo\"");
header("Link: <   http://wut.com.test/>; rel=preconnect", false);
?>
<!DOCTYPE html>
<script>
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
</script>
This test check if a Link header triggered a dns prefetch, after an invalid header.
<script>
    if (window.testRunner)
        testRunner.notifyDone();
</script>
