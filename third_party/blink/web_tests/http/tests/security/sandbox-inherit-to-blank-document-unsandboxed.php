<?php
header("Content-Security-Policy: sandbox allow-scripts allow-popups allow-popups-to-escape-sandbox");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>
</head>
<body>
    <a target='_blank' rel="opener" href='/security/resources/post-origin-to-opener.html'></a>
    <script>
        if (window.testRunner) {
            testRunner.setCanOpenWindows();
        }

        var test = async_test("Testing sandbox not inherited via target='_blank' when 'allow-popups-to-escape-sandbox' present");

        window.addEventListener("message", test.step_func(function (e) {
            assert_equals(self.origin, 'null');
            assert_equals(e.data.origin, 'http://127.0.0.1:8000');
            test.done();
        }));

        test.step(function () {
            var link = document.querySelector('a');
            link.click();
        });
    </script>
</body>
</html>
