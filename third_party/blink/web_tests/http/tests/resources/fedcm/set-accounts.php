<?php
$http_origin = $_SERVER["HTTP_ORIGIN"] ?? null;
header("Access-Control-Allow-Credentials: true");
header("Access-Control-Allow-Origin: " . $http_origin);

if (isset($_GET["noaccounts"])) {
  // We can't uset setcookie() because the bundled PHP on Wiondows is too old
  // to support setting SameSite.
  header("Set-Cookie: noaccounts=1; SameSite=None; Secure");
} else if (isset($_GET["default"])) {
  setcookie("noaccounts", "", 0);
} else {
  header("HTTP/1.1 500 No parameter");
  echo("No/wrong parameter");
}
?>
