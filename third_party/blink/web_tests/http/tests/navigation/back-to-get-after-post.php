<?php
if ($_SERVER['REQUEST_METHOD'] == "POST") {
    header("Location: resources/back-to-get-after-post-helper.html");
    exit;
}
?>
<form method=post>
<input type=submit></input>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpBackForwardList();
    testRunner.waitUntilDone();
}

window.onpageshow = function() {
    setTimeout(function() {
        if (sessionStorage.getItem("backToGet") == null) {
            sessionStorage.setItem("backToGet", "step1");
            document.forms[0].submit();
        } else if (sessionStorage.getItem("backToGet") == "step2") {
            sessionStorage.setItem("backToGet", "step3");
            history.back();
        } else {
            sessionStorage.removeItem("backToGet");
            if (window.testRunner)
                testRunner.notifyDone();
        }
    }, 0);
};
</script>
</form>
