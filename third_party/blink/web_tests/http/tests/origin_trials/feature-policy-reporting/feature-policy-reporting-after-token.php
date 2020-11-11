<?php
# This test validates that feature policy report-only mode works when enabled
# by origin trial, with the policy header appearing after the OT header.

# Generate token with the command:
# generate_token.py http://127.0.0.1:8000 FeaturePolicyReporting --expire-timestamp=2000000000
header("Origin-Trial: ApBWaWympGv9yGwIbWoVDwk25NAT5Zkpfs2nx9PIM0GSXGcLHanrrJQteJgylNk45tun6wJjWUKBZoEsa+BzQgAAAABeeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRmVhdHVyZVBvbGljeVJlcG9ydGluZyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
header("Feature-Policy-Report-Only: sync-xhr 'none'");
?>
<title>Feature Policy Report-Only - test definition after trial token</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
const check_report_format = ([reports, observer]) => {
  const report = reports[0];
  assert_equals(report.type, "permissions-policy-violation");
  assert_equals(report.body.featureId, "sync-xhr");
  assert_equals(report.body.disposition, "report");
};

promise_test(async t => {
  const report = new Promise(resolve => {
    new ReportingObserver((reports, observer) => resolve([reports, observer]),
                          {types: ['permissions-policy-violation']}).observe();
  });
  const xhr = new XMLHttpRequest();
  xhr.open("GET", document.location.href, false);
  xhr.send();
  check_report_format(await report);
}, "Feature policy report only mode defined after origin trial token");
</script>
