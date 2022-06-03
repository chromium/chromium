<?php
header("Referrer-Policy: no-referrer");
?>
<!DOCTYPE html>
<head>
  <!-- An invalid meta referrer policy should be ignored. -->
  <meta name="referrer" content="origin,unsafe-url">
  <script src="/resources/testharness.js"></script>
  <script src="/resources/testharnessreport.js"></script>
  <script src="/resources/get-host-info.js"></script>
</head>
<body>
</body>
<script>
    var policy = "no-referrer";
    var expectedReferrer = "";
    var navigateTo = "same-origin";
</script>
<script src="resources/header-test.js"></script>
</html>
