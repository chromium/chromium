<?php
  $writeValue = $_GET['write'] ?? '';

  // We need live headers, so make sure it's not cached.
  header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
  header('Shared-Storage-Write: ' . $writeValue);
?>
<html>
<body>
Test writing to shared storage
</body>
</html>