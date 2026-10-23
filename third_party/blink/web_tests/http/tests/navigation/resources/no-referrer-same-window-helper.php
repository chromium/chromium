<?php
    // Prevent from being cached.
    header("Cache-Control: no-store, private, max-age=0");
    header("Content-Type: text/html");
?>

<html><body>
The Referrer displayed below should be empty.<br/>
<div id="referrer">Referrer: <?php echo $_SERVER['HTTP_REFERER']; ?></div>
<div id="console"></div>
<script>
    var line = document.createElement('div');
    line.appendChild(document.createTextNode(document.getElementById("referrer").innerText == "Referrer:" ? "PASS" : "FAIL"));
    document.getElementById('console').appendChild(line);
    
    if (window.testRunner)
        testRunner.notifyDone();
</script>
</body></html>
