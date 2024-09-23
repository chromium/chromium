<?php
if (isset($_COOKIE['back-to-redirect-with-frame'])) {
    header('Location: resources/pass-and-notify-done.html');
    exit;
}

header("Cache-Control: no-store");
header("Set-Cookie: back-to-redirect-with-frame = true;");
?>
<body>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpBackForwardList();
    testRunner.waitUntilDone();
}

window.onload = function() {
    // A |setTimeout| is required here. Blink makes non-user initiated
    // navigations triggered before and during window.onload to replace the
    // current entry.
    setTimeout(function() {
        window.location = "resources/go-back.html";
    }, 500);
};
</script>
<iframe src="about:blank"></iframe>
</body>
