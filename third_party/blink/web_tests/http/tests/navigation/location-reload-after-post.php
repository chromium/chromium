<body>
<form method=post>
<input type=submit></input>
</form>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpFrameLoadCallbacks();
    testRunner.waitUntilDone();
}

window.onload = function() {
    setTimeout(function() {
        if (sessionStorage.getItem("reloadAfterPost") == null) {
            sessionStorage.setItem("reloadAfterPost", "step1");
            document.forms[0].submit();
        } else if (sessionStorage.getItem("reloadAfterPost") == "step1") {
            sessionStorage.setItem("reloadAfterPost", "step2");
            location.reload();
        } else {
            sessionStorage.removeItem("reloadAfterPost");
            if (window.testRunner)
                testRunner.notifyDone();
        }
    }, 0);
};

</script>
</body>
