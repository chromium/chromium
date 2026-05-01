<?php
header("Content-Security-Policy-Report-Only: script-src 'self' 'unsafe-inline'");
?>

<!DOCTYPE html>

<meta http-equiv="Content-Security-Policy" content="script-src 'self' 'unsafe-inline'; font-src 'none'">
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>

<link rel=preload href="../resources/Ahem.ttf" as=font crossorigin>

<script>
var t = async_test("Ensure CSP meta tags properly block preloads when there are also CSP tags in the response headers");
window.addEventListener("load", t.step_func(function() {
  if (window.internals) {
    assert_true(internals.isPreloaded('../resources/testharness.js'));
    assert_true(internals.isPreloaded('../resources/testharnessreport.js'));
    assert_false(internals.isPreloaded('../resources/Ahem.ttf'), "fonts should not be preloaded");
    t.done();
  }
}));
</script>

