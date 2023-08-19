<?php
header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<head>
<script>

function runTest()
{
    var r = document.getElementById('result');
    var o = document.getElementById('output').firstChild;
    if (o.nodeValue == '\u2122\u5341') 
        r.innerHTML = "SUCCESS: query param is converted to UTF-8";
    else
        r.innerHTML = "FAILURE: query param is not converted to UTF-8. value=" +
        o.nodeValue;
        
    if (window.testRunner)
        testRunner.notifyDone();
}

</script>
</head>
<body onload="runTest()">
<p>
This test is for <a href="http://bugs.webkit.org/show_bug.cgi?id=21635">bug 21635</a>. The query parameter in non-UTF-8 Unicode pages (UTF-7,16,32) should be converted to UTF-8 before a request is made to a server.
</p>
<div style='display: none;' id='output'><?php $q = $_REQUEST['q'] ?? ''; echo $q; ?></div>
<div id="result"></div>
</body>
</html>
