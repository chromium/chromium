<?php
header("Link: <script>; rel=preload; as=script", false);
header("Link: <image>; rel=preload; as=image", false);
header("Link: <font>; rel=preload; as=font", false);
header("Link: <stylesheet>; rel=preload; as=style", false);
header("Link: <fetch>; rel=preload; as=fetch", false);
header("Link: <script_auto>; rel=preload; as=script; fetchpriority=auto", false);
header("Link: <script_low>; rel=preload; as=script; fetchpriority=low", false);
header("Link: <image_high>; rel=preload; as=script; fetchpriority=high", false);
header("Link: <image_invalid>; rel=preload; as=script; fetchpriority=invalid", false);
?>
<!DOCTYPE html>
<script>
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
    if (window.internals) {
        internals.settings.setLogPreload(true);
    }
    if (!localStorage.getItem("reloaded")) {
        localStorage.setItem("reloaded",  true);
        location.reload();
    } else {
        localStorage.removeItem("reloaded");
    }
</script>
This test checks if a Link header triggered preloads with the correct fetchpriority hint value.
<script>
    if (window.testRunner)
        testRunner.notifyDone();
</script>
