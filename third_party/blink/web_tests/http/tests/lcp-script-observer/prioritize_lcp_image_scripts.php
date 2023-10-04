<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
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

  hint.lcpInfluencerScripts = [
    {url: new URL('/lcp-script-observer/resources/lcp-img-insert.js', location).toString()},
    {url: new URL('/lcp-script-observer/resources/lcp-img-create.js', location).toString()}
  ];
  // All fields are non-nullable.
  hint.lcpElementLocators = [];
  hint.fetchedFonts = [];

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
<script src="/lcp-script-observer/resources/inert.js" async></script>
<script src="/lcp-script-observer/resources/lcp-img-create.js" async></script>
<script src="/lcp-script-observer/resources/lcp-img-insert.js" async></script>
<script>
  assert_implements(window.LargestContentfulPaint, "LargestContentfulPaint is not implemented");

  promise_test(async t => {
    const url = new URL('/lcp-script-observer/resources/lcp-img-insert.js', location).toString();
    const hint_matched_script1_priority = await internals.getInitialResourcePriority(url, document);
    assert_equals(hint_matched_script1_priority, kVeryHigh);

    const url2 = new URL('/lcp-script-observer/resources/lcp-img-create.js', location).toString();
    const hint_matched_script2_priority = await internals.getInitialResourcePriority(url2, document);
    assert_equals(hint_matched_script2_priority, kVeryHigh);
  }, "Ensure LCPP hinted async scripts were loaded with VeryHigh priority.")

  promise_test(async t => {
    const url = new URL('/lcp-script-observer/resources/inert.js', location).toString();
    const non_hint_matched_script_priority = await internals.getInitialResourcePriority(url, document);

    assert_equals(non_hint_matched_script_priority, kLow);
  }, "Ensure non-LCPP hinted async scripts were loaded unaffected with Low priority.")
</script>
