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
    <script>
        if (window.testRunner) {
            testRunner.setPopupBlockingEnabled(false);
        }

        var test = async_test("Testing that popups opened when 'allow-popups-to-escape-sandbox' present can be navigated");

        var current = 0;
        var win = null;

        window.addEventListener("message", test.step_func(function (e) {
            assert_equals(self.origin, 'null');
            if (current == 0) {
                assert_equals(e.data.origin, 'http://127.0.0.1:8000');
                current++;
                win.location.replace('http://localhost:8000/security/resources/post-origin-to-opener.html');
            } else {
                assert_equals(e.data.origin, 'http://localhost:8000');
                test.done();
            }
        }));

        test.step(function () {
            console.log("Opening a window!");
            win = window.open('/security/resources/post-origin-to-opener.html', '_blank', '');
        });
    </script>
</body>
</html>
