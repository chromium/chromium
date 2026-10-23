<?php
$mime = $_GET['mime'] ?? '';
if ($mime) {
  header("Content-Type: " . $mime);
} else {
  // Set a header and remove it to override the default 'text/html'.
  header("Content-Type:");
  header_remove("Content-Type");
}
?>
