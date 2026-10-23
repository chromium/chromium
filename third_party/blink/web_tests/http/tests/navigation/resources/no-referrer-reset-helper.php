<?php
    // Prevent from being cached.
    header("Cache-Control: no-store, private, max-age=0");
    header("Content-Type: text/html");
?>

<html><body>
<div id="referrer">
Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?>
</div>
<br/>
<a id="noreferrer-link" href="no-referrer-reset-helper.php" rel="noreferrer">Doesn't send referrer</a>
<a id="referrer-link" href="no-referrer-helper.php">Sends referrer</a>
<script>
    var consoleWindow = window.open("", "consoleWindow");
    if (consoleWindow) {
        consoleWindow.log(document.getElementById("referrer").innerText);
        consoleWindow.log("window.opener: " + (window.opener ? window.opener.location : ""));
    }

    var target;
    if (!consoleWindow.noreferrerStepDone) {
        consoleWindow.noreferrerStepDone = true;
        target = document.getElementById("noreferrer-link");
    } else 
        target = document.getElementById("referrer-link");
    var newEvent = document.createEvent("MouseEvent");
    newEvent.initMouseEvent("click", false, false, window, 0, 10, 10, 10, 10, false, false, false, false, 0, target);
    target.dispatchEvent(newEvent);
</script>
</body></html>
