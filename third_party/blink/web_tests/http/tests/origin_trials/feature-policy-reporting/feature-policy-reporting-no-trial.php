<?php
# This test validates that no reports are generated when the trial is disabled.
# This will fail when reporting is enabled any other way (in layout tests
# outside of virtual/stable, for instance).
header("Feature-Policy-Report-Only: sync-xhr 'none'");
?>
<title>Feature Policy Report-Only - test reporting without a trial token</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="/reporting-observer-resources/intervention.js"></script>
<div id="target" style="padding: 10px; background-color: blue;">
  <p>Testing intervention reports and ReportingObserver</p>
</div>
<script>
const check_report_format = ([reports, observer]) => {
  const report = reports[0];
  assert_equals(report.type, "intervention");
};

promise_test(async t => {
  const report = new Promise(resolve => {
      new ReportingObserver((reports, observer) => resolve([reports, observer]),
                            {types: ['permissions-policy-violation','intervention']}).observe();
        });

  const xhr = new XMLHttpRequest();
  xhr.open("GET", document.location.href, false);
  xhr.send();

  // This will cause an intervention report to be generated. If feature policy
  // reporting is correctly disabled, then this will be the first report seen
  // by the observer.
  causeIntervention();

  check_report_format(await report);
}, "Feature policy report only mode without the corresponding origin trial")


</script>
