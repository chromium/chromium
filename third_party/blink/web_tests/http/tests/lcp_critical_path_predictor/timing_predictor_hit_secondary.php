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

  var getLCPBytes = async function(proto_file) {
    const resp = await fetch("/gen/third_party/blink/renderer/core/lcp_critical_path_predictor/test_proto/" + proto_file);
    const bytes = new ByteString;
    bytes.data = new Uint8Array(await resp.arrayBuffer());
    return bytes;
  };

  hint.lcpElementLocators = [
      await getLCPBytes("lcp_image_id.pb"),
      await getLCPBytes("lcp_image_id_b.pb")
  ];
  // All fields are non-nullable.
  hint.lcpInfluencerScripts = [];
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
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
  promise_test(async t => {
    const lcp_elment = await internals.LCPPrediction(document);
    assert_equals(lcp_elment, "/#lcp_image_b");
  }, "Ensure document::RunLCPPredictedCallbacks is called with the 2nd LCP element locator.")
</script>
<img src="/resources/square.png" id="lcp_image_b">
<img src="/resources/square100.png">
