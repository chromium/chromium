<?php
// Generate this token with the command:
// generate_token.py http://127.0.0.1:8000 StorageFoundationAPI --expire-timestamp=2000000000
header("Origin-Trial: A3C2zYg350joPm2nXnW161G1NPsfxe5lianKdg5qM1a/qjuxGRoQDaZk0mGxl9ErHv/iXEfZ0f3lskbjeqFuFggAAABceyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiU3RvcmFnZUZvdW5kYXRpb25BUEkiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<title>Test that StorageFoundationAPI trial is enabled by header when navigated by window.open</title>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script src="../resources/window-open-helper.js"></script>
<script>
setupWindowOpenTest();

function runTest() {
  runTestInTarget(() => {
        return (typeof storageFoundation == 'object');
      },
      'storageFoundation should be defined in document');
}
</script>
<html>
<body onload="runTest()">
  <p>
    This test opens a new window. It passes, if the trial is enabled in the
    opened window.
  </p>
  <script>
    openCurrentAsTarget();
  </script>
</body>
</html>
