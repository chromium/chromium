<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {mojo} from "/gen/mojo/public/js/bindings.js";
import {NonAssociatedWebTestControlHostRemote} from "/gen/content/web_test/common/web_test.mojom.m.js";
import {ByteString} from "/gen/mojo/public/mojom/base/byte_string.mojom.m.js";
import {LCPCriticalPathPredictorNavigationTimeHint} from "/gen/third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.m.js";

if (!window.testRunner) {
  console.log("This test requires window.testRunner.")
}

testRunner.dumpAsText();
testRunner.waitUntilDone();
if (window.location.search != "?start") {
  const hint = new LCPCriticalPathPredictorNavigationTimeHint();

  const resp = await fetch("/gen/third_party/blink/renderer/core/lcp_critical_path_predictor/test_proto/lcp_image_id.pb");

  hint.fetchedFonts = [
    {url: new URL('/resources/Ahem.ttf', location).toString()},
  ];
  // All fields are non-nullable.
  hint.lcpElementLocators = [];
  hint.lcpInfluencerScripts = [];

  const web_test_control_host_remote = new NonAssociatedWebTestControlHostRemote();
  web_test_control_host_remote.$.bindNewPipeAndPassReceiver().bindInBrowser('process');
  web_test_control_host_remote.setLCPPNavigationHint(hint);

  window.location.search = '?start';
}
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
