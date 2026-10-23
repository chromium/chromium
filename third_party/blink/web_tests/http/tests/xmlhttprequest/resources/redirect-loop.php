<?php
header("HTTP/1.1 307");
header("Access-Control-Allow-Origin: *");

$url = $_GET['Redirect'] ?? '';
$path = '/xmlhttprequest/resources/redirect-loop.php';
$count = intval($_GET['Count'] ?? 0) - 1;
if ($count >= 0) {
  $url = $path .
         '?Redirect=' . rawurlencode($url) .
         '&Count=' . $count ;
  header("Location: $url");
} else {
  echo "PASS";
}
?>
