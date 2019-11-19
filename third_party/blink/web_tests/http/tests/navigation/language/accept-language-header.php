<html>
<body>
<p>Tests the default Accept-Language header</p>
<script>
    if (window.testRunner) {
        testRunner.dumpAsText();
    }

    let accept_language = '<?php echo $_SERVER['HTTP_ACCEPT_LANGUAGE']; ?>';
    document.write("HTTP Accept-Language header should be: " + accept_language + "<br>");
</script>
</body>
</html>