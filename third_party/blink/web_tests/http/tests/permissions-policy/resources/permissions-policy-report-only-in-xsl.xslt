<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="page">
  <html>
    <head>
      <title> Test XSLT </title>
      <script src="../../resources/testharness.js"></script>
      <script src="../../resources/testharnessreport.js"></script>
      <script>
        // 'geolocation' is enabled in the page but is disabled
        // in Permissions-Policy-Report-Only header.
        // A permissions-policy-violation report is expected.

        async_test(t => {
          new ReportingObserver(t.step_func_done((reports, _) => {
            assert_equals(reports.length, 1);
            const report = reports[0];
            assert_equals(report.type, 'permissions-policy-violation');
            assert_equals(report.body.featureId, 'geolocation');
            assert_equals(report.body.disposition, 'report');
          }), {types: ['permissions-policy-violation']}).observe();
        });
        navigator.geolocation.getCurrentPosition(_ => {});
      </script>
    </head>
    <body bgcolor="#ffffff">
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>