<?php
  header("Content-Security-Policy: treat-as-public-address");
?><!doctype html>
<html>
<head>
    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>
    <script src="./resources/preflight.js"></script>
</head>
<body>
    <script>
        async_test(function (t) {
            var xhr = new XMLHttpRequest;
            xhr.onload = t.unreached_func("The load should fail.");
            xhr.onerror = t.step_func_done(function (e) {
                assert_equals(0, e.loaded);
            });

            xhr.open("GET", preflightURL('fail-with-500', 'json'), true);
            xhr.send();
        }, "XHR should fail on failed preflight: 500 status");

        async_test(function (t) {
            var xhr = new XMLHttpRequest;
            xhr.onload = t.unreached_func("The load should fail.");
            xhr.onerror = t.step_func_done(function (e) {
                assert_equals(0, e.loaded);
            });

            xhr.open("GET", preflightURL('fail-without-allow', 'json'), true);
            xhr.send();
        }, "XHR should fail on failed preflight: no allow-external");


        async_test(function (t) {
            var xhr = new XMLHttpRequest;
            xhr.responseType = "json";
            xhr.onload = t.step_func_done(function (e) {
                assert_equals('success', xhr.response.jsonpResult);
            });
            xhr.onerror = t.unreached_func("The load should not fail.");

            xhr.open("GET", preflightURL('pass', 'json'), true);
            xhr.send();
        }, "XHR should pass on successful preflight");
    </script>
</body>
</html>
