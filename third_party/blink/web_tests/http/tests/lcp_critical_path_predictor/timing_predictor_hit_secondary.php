<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {setupLCPTest} from "./resources/common.js";
await setupLCPTest(["lcp_image_id.pb", "lcp_image_id_b.pb"]);
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
    const lcp_element = await internals.LCPPrediction(document);
    assert_equals(lcp_element, "/#lcp_image_b");
  }, "Ensure document::RunLCPPredictedCallbacks is called with the 2nd LCP element locator.")
</script>
<img src="/resources/square.png" id="lcp_image_b">
<img src="/resources/square100.png">
