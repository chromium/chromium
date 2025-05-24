<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {setupLCPTest} from "../lcp_critical_path_predictor/resources/common.js";
await setupLCPTest({
  fetchedFonts : [
    {url: new URL('/resources/Ahem.ttf', location).toString()},
  ]
});
</script>
<?php
// Do not output the HTML below this PHP block until the test is reloaded with
// "?start" to avoid it being picked up by the HTMLPreloadScanner.
if ($_SERVER['QUERY_STRING'] != "start")
  exit;
?>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
  promise_test(async t => {
    const url = new URL('/resources/Ahem.ttf', location).toString();
    const is_preloaded = await internals.isPreloaded(url);

    assert_true(is_preloaded, "Hinted font should be preloaded");
  }, "Ensure LCPP hinted fonts were preloaded.")

  promise_test(async t => {
    const url = new URL('/resources/variabletest_box.ttf', location).toString();
    const is_preloaded = await internals.isPreloaded(url);

    assert_false(is_preloaded, "Non-hinted font should not be preloaded");
  }, "Ensure non-LCPP hinted fonts were not preloaded.")
</script>
<style type="text/css">
@font-face {
  font-family: 'Test0';
  src: url('/resources/Ahem.ttf') format('truetype');
}
@font-face {
  font-family: 'Test1';
  src: url('/resources/variabletest_box.ttf') format('truetype');
}

h1 {
  font-family: 'Test0', sans-serif;
}
h2 {
  font-family: 'Test1', sans-serif;
}
</style>
<h1>Test0</h1>
<h2>Test1</h2>
