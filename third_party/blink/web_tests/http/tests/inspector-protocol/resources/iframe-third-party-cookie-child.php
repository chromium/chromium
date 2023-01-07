<?php
header('Content-Type: text/html; charset=UTF-8');
if(isset($_GET['opaque'])) {
  header('Set-Cookie: __Host-foo=bar; Secure; Path=/; SameSite=None; Partitioned');
}
?>
