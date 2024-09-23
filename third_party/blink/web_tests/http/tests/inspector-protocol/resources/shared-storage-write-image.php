<?php
  $writeValue = $_GET['write'] ?? '';

  // We need live headers, so make sure it's not cached.
  header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
  header('Content-Type: image/png');
  header('Shared-Storage-Write: ' . $writeValue);

  $imageData = file_get_contents('image.png');
  echo $imageData;
?>