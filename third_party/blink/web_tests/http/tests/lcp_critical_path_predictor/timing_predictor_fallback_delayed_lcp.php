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
    var lcp_callback_done = false;
    let lcp_promise =  new Promise(async (res)=> {
      let lcp_element = await internals.LCPPrediction(document);
      lcp_callback_done = true;
      assert_equals(lcp_element, "", "LCP prediction should be called as fallback.");
      assert_true(img_element.complete, "image should be loaded.");
      res();
    });

    // Make sure window.onload > image.onload > prediction fallback.
    await new Promise((res) => {window.onload = res;});
    assert_false(lcp_callback_done, "LCP fallback should not be called before image load.");

    var img_element = document.createElement("img");
    img_element.src = "/resources/square.png";
    document.body.appendChild(img_element);

    return lcp_promise;
  }, "Ensure document::RunLCPPredictedCallbacks is called if no LCP before window.onload and when the first LCP occurs after that.")
</script>