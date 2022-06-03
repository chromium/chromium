<?php
header("Permissions-Policy: document-domain=()");
?>
<!DOCTYPE html>

<head>
  <script src="/resources/testharness.js"></script>
  <script src="/resources/testharnessreport.js"></script>
  <script>
    function navigateToJS() {
      const script = 'script';
      const testPage = `
<html>
<head>
  <title> Test JavaScript URL Navigation </title>
  <${script} src="/resources/testharness.js"></${script}>
  <${script} src="/resources/testharnessreport.js"></${script}>
  <${script}>
    test(() => {
      let feature_allowed;
      try {
        document.domain = document.domain;
        feature_allowed = true;
      } catch(e) {
        feature_allowed = false;
      }

      assert_false(feature_allowed, "Feature(Document Domain) should not be allowed by permissions policy.");
    });
  </${script}>
</head>

</html>
`;

      window.location.href = `javascript: '${testPage}'`;
    }
  </script>
</head>

<body onload=navigateToJS()>
</body>