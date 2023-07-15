<?php
if (isset($_GET["Cookie"])) {
  $cookie_check = $_GET["Cookie"];
  if ($cookie_check == "NotSet") {
    if (isset($_COOKIE['TestCookie'])) {
      header("HTTP/1.0 404 Not Found");
      exit;
    }
  } else if ($cookie_check != $_COOKIE['TestCookie']) {
    header("HTTP/1.0 404 Not Found");
    exit;
  }
}

header("Access-Control-Allow-Origin: *");

$name = 'abe.png';
$fp = fopen($name, 'rb');
header("Content-Type: image/png");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
