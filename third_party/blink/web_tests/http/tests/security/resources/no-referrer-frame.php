<script>
function log(message)
{
    parent.document.getElementById("log").innerHTML += message + "<br>";
}

if (document.referrer.toString() != "") {
  log("JavaScript: FAIL");
} else {
  log("JavaScript: PASS");
}

<?php
$refer = $_SERVER['HTTP_REFERER'] ?? null;
if ($refer && $refer != "")
    print("log('HTTP Referer: FAIL')");
else
    print("log('HTTP Referer: PASS')");
?>

window.onload = function() {
    var xhr = new XMLHttpRequest;
    xhr.open("GET", "no-referrer.php", false);
    xhr.send(null);
    log("Sync XHR: " + (xhr.responseText.match(/HTTP.*FAIL/) ? "FAIL" : "PASS"));
    xhr.open("GET", "no-referrer.php", true);
    xhr.send(null);
    xhr.onload = onXHRLoad;
}

function onXHRLoad(evt)
{
    log("ASync XHR: " + (evt.target.responseText.match(/HTTP.*FAIL/) ? "FAIL" : "PASS"));
    log("DONE");
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<script src="no-referrer.php"></script>
