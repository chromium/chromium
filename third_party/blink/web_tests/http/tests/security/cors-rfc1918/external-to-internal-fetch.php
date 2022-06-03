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
        promise_test(function (t) {
            return promise_rejects_js(t, TypeError, fetch(preflightURL('fail-with-500', 'json')));
        }, "'fetch()' should fail on failed preflight: 500 status");

        promise_test(function (t) {
            return promise_rejects_js(t, TypeError, fetch(preflightURL('fail-without-allow', 'json')));
        }, "'fetch()' should fail on failed preflight: no allow-external");

        promise_test(function (t) {
            return fetch(preflightURL('pass', 'json'))
                .then(function (response) {
                    return response.json();
                }).then(function (json) {
                    assert_equals('success', json.jsonpResult);
                });
        }, "'fetch()' should pass on successful preflight");
    </script>
</body>
</html>
