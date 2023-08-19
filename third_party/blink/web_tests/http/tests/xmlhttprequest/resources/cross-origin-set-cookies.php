<?php
$age_string = "";
if (isset($_GET['clear'])) {
    $age_string = "; expires=Thu, 19 Mar 1982 11:22:11 GMT";
}
header("Set-Cookie: WK-xhr-cookie-storage=MySpecialValue" . $age_string .
       "; SameSite=None; Secure");
header("Cache-Control: no-store");
header("Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT");
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");
?>
log('PASS: Loaded');
