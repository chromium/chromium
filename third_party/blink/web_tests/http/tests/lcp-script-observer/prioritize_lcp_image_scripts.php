<!doctype html>
<script src="/priorities/resources/common.js"></script>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script type=module>
import {setupLCPTest} from "../lcp_critical_path_predictor/resources/common.js";
await setupLCPTest({
  lcpInfluencerScripts : [
    {url: new URL('/lcp-script-observer/resources/lcp-img-insert.js', location).toString()},
    {url: new URL('/lcp-script-observer/resources/lcp-img-create.js', location).toString()}
  ]
});
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
