<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script type=module>
import {setupLCPTest} from "./resources/common.js";
await setupLCPTest();
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
  var lcp_callback_done = false;
  var lcp_promise =  new Promise(async (res)=> {
    let lcp_element = await internals.LCPPrediction(document);
    lcp_callback_done = true;
    assert_equals(lcp_element, "", "LCP prediction should be called as fallback.");
    assert_true(img_element.complete, "image should be loaded.");
    res();
  });
</script>
<img src="/resources/square.png" id="lcp_image">
<script>
  var img_element = document.getElementById("lcp_image");
  promise_test(async t => {
    // msan or some slow env can make img load finished already.
    if (!img_element.complete) {
      await new Promise( (res) =>{img_element.onload = res;});
    }
    assert_false(lcp_callback_done, "LCP fallback should not be called before window load.");

    return lcp_promise;
  }, "Ensure document::RunLCPPredictedCallbacks is called even no LCP element locator hit at window.onload.");
</script>