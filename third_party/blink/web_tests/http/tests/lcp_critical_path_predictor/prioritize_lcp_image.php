<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {setupLCPTest} from "./resources/common.js";
await setupLCPTest(["lcp_image_id.pb"]);
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
    const url = new URL('/resources/square.png', location).toString();
    const hint_matched_img_priority = await internals.getInitialResourcePriority(url, document);

    assert_equals(hint_matched_img_priority, kVeryHigh);
  }, "Ensure LCPP hinted images were loaded with VeryHigh priority.")

  promise_test(async t => {
    const url = new URL('/resources/square100.png', location).toString();
    const hint_matched_img_priority = await internals.getInitialResourcePriority(url, document);

    assert_equals(hint_matched_img_priority, kMedium);
  }, "Ensure non-LCPP hinted images were loaded unaffected with Medium priority.")

  promise_test(async t => {
    const lcp_element = await internals.LCPPrediction(document);
    assert_equals(lcp_element, "/#lcp_image");
  }, "Ensure document::RunLCPPredictedCallbacks is called with LCP element locator.")
</script>
<img src="/resources/square.png" id="lcp_image">
<img src="/resources/square100.png">
