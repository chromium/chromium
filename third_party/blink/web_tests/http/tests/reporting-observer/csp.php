<?php
header("Content-Security-Policy: img-src 'none';");
?>
<!doctype html>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>

<img id="target">

<script>
async_test(function(test) {
  var observer = new ReportingObserver(function(reports, observer) {
    test.step(function() {
      assert_equals(reports.length, 1);

      // Ensure that the contents of the report are valid.
      assert_equals(reports[0].type, "csp-violation");
      assert_true(reports[0].url.endsWith("reporting-observer/csp.php"));
      assert_true(reports[0].body.documentURL.endsWith(
          "reporting-observer/csp.php"));
      assert_equals(reports[0].body.referrer, "");
      assert_true(reports[0].body.blockedURL.endsWith(
          "reporting-observer/fail.png"));
      assert_equals(reports[0].body.effectiveDirective, "img-src");
      assert_equals(reports[0].body.originalPolicy,
                    "img-src 'none';");
      assert_equals(reports[0].body.sourceFile, null);
      assert_equals(reports[0].body.sample, "");
      assert_equals(reports[0].body.disposition, "enforce");
      assert_equals(reports[0].body.statusCode, 200);
      assert_equals(reports[0].body.lineNumber, null);
      assert_equals(reports[0].body.columnNumber, null);
      // Ensure the toJSON call is successful.
      const reportJSON = reports[0].toJSON();
      assert_equals(reportJSON.type, reports[0].type);
      assert_equals(reportJSON.url, reports[0].url);
      assert_equals(typeof reportJSON.body, "object");
      assert_equals(reportJSON.body.documentURL, reports[0].body.documentURL);
      assert_equals(reportJSON.body.referrer, reports[0].body.referrer);
      assert_equals(reportJSON.body.blockedURL, reports[0].body.blockedURL);
      assert_equals(reportJSON.body.effectiveDirective, reports[0].body.effectiveDirective);
      assert_equals(reportJSON.body.originalPolicy, reports[0].body.originalPolicy);
      assert_equals(reportJSON.body.sourceFile, reports[0].body.sourceFile);
      assert_equals(reportJSON.body.sample, reports[0].body.sample);
      assert_equals(reportJSON.body.disposition, reports[0].body.disposition);
      assert_equals(reportJSON.body.statusCode, reports[0].body.statusCode);
      assert_equals(reportJSON.body.lineNumber, reports[0].body.lineNumber);
      assert_equals(reportJSON.body.columnNumber, reports[0].body.columnNumber);
      // Ensure that report can be successfully JSON serialized.
      assert_equals(JSON.stringify(reports[0]), JSON.stringify(reportJSON));
    });

    test.done();
  });
  observer.observe();

  // Attempt to load an image, which is disallowed by the content security
  // policy. This will generate a csp-violation report.
  document.getElementById("target").src = "fail.png";
}, "CSP Report");
</script>
