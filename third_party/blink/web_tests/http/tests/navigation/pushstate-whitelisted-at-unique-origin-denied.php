<?php
header("Content-Security-Policy: sandbox allow-scripts");
?>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script>
var orginURL = document.URL;
test(function () {
    testRunner.addOriginAccessAllowListEntry(location.origin, location.protocol, '', false);
}, 'testRunner.addOriginAccessAllowListEntry is required for this test');

test(function () {
    assert_throws_dom('SecurityError', function () {
        history.pushState(null, null, orginURL + "/path");
    });
}, 'pushState at unique origin should fail with SecurityError (even with whitelisted origins)');

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
