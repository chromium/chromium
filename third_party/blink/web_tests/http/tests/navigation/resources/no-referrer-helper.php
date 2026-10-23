<?php
    // Prevent from being cached.
    header("Cache-Control: no-store, private, max-age=0");
    header("Content-Type: text/html");
?>

<html><body>
<div id="console"></div>
<div id="referrer">
Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?>
</div>
<br/>
<script>
function log(msg)
{
    var line = document.createElement('div');
    line.appendChild(document.createTextNode(msg));
    document.getElementById('console').appendChild(line);
}

    var consoleWindow = window.open("", "consoleWindow");
    if (consoleWindow && consoleWindow.log) {
        consoleWindow.log(document.getElementById("referrer").innerText);
        consoleWindow.log("window.opener: " + (window.opener ? window.opener.location : ""));
    }
    
    if (window.testRunner)
        testRunner.notifyDone();
</script>

</body></html>
