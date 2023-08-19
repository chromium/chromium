<html>
<head>
<script>
function log(msg) {
    document.getElementById("log").innerHTML += msg + "<br>";
}

function runTest() {
    var referrerHeader = "<?php echo ($_SERVER['HTTP_REFERER'] ?? null) ?>";
    if (referrerHeader == "")
        log("HTTP Referer header is empty");
    else
        log("HTTP Referer header is " + referrerHeader);

    if (document.referrer == "")
        log("Referrer is empty");
    else
        log("Referrer is " + document.referrer);

    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
</head>
<body onload="runTest()">
<div id="log"></div>
</body>
</html>
