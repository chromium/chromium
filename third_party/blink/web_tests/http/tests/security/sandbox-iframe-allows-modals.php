<?php
header("Content-Security-Policy: sandbox allow-scripts allow-modals");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>
    <script>
      // testharnessreport.js disables reporting js dialogs so turn it back on so that we can
      // detect they were shown via the -expected.txt output.
      if (testRunner)
        testRunner.setDumpJavaScriptDialogs(true);
    </script>
</head>
<body>
    <script>
        test(function () {
            var result = alert("Yay!");
            assert_equals(result, undefined);
        }, "alert() is shown.");

        test(function () {
            var result = print();
            assert_equals(result, undefined);
        }, "print() does get called.");

        test(function () {
            var result = confirm("Question?");
            assert_equals(result, true);
        }, "confirm() returns 'true' (in our test environment).");

        test(function () {
            var result = prompt("Question?");
            assert_equals(result, "");
        }, "prompt() returns a result.");
    </script>
</body>
</html>
