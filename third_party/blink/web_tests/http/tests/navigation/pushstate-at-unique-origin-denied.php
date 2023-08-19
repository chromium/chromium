<?php
header("Content-Security-Policy: sandbox allow-scripts");
?>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script>
var orginURL = document.URL;
test(function () {
    try {
        history.pushState(null, null, orginURL + "/path");
        done();
    } catch (e) {
        assert_unreached("pushState #hash should not fail.");
    }
}, 'pushState /path in unique origin should not fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, orginURL + "#hash");
        done();
    } catch (e) {
        assert_unreached("pushState #hash should not fail.");
    }
}, 'pushState #hash in unique origin should not fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, orginURL + "?hash");
        done();
    } catch (e) {
        assert_unreached("pushState ?hash should not fail.");
    }
}, 'pushState ?hash in unique origin should not fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, orginURL + "?hash#base");
        done();
    } catch (e) {
        assert_unreached("pushState ?hash#base should not fail.");
    }
}, 'pushState ?hash#base in unique origin should not fail with SecurityError');
</script>
