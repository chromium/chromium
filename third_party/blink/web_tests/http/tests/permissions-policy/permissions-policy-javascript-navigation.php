<?php
header("Permissions-Policy: geolocation=()");
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
    test(t => {
      navigator.geolocation.getCurrentPosition(
          t.step_func_done(),
          t.unreached_func(
              "Feature(Geolocation) should not be allowed by permissions policy.")
      );
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