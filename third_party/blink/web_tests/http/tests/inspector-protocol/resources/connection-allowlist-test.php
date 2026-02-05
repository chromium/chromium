<?php
  header("Content-Type: text/html");
  header("Access-Control-Allow-Origin: *");

  if (isset($_GET["enforced"])) {
    header("Connection-Allowlist: {$_GET["enforced"]}");
  }

  if (isset($_GET["report_only"])) {
    header("Connection-Allowlist-Report-Only: {$_GET["report_only"]}");
  }
?>
<!DOCTYPE html>
<title>Connection-Allowlist Test</title>
<p>Hello World</p>
