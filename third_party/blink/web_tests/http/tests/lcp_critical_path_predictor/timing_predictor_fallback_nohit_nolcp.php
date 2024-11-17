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
  promise_test(async t => {
    var lcp_promise =  new Promise(async (res)=> {
      let lcp_element = await internals.LCPPrediction(document);
      assert_equals(lcp_element, "", "LCP prediction should be called as fallback.");

      assert_false(has_lcp_occured, "No LCP should happen.");
      res();
    });

  var has_lcp_occured = false;
  const observer = new PerformanceObserver((list) => {
    has_lcp_occured |= (list.length > 0);
  });
  observer.observe({ type: "largest-contentful-paint", buffered: true });

    // Make sure window.onload > prediction fallback.
    await new Promise((res) => {window.onload = res;});

    return lcp_promise;
  }, "Ensure document::RunLCPPredictedCallbacks is called even no learned LCP element locator and no LCP element at window.onload.");
</script>