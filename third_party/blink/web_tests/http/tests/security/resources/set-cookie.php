<?php
$set_cookie_header_string = 'Set-Cookie: ' . $_GET["name"] . '=' .
                            $_GET["value"] . '; Path=/';
if (isset($_GET["SameSiteNone"]))
  $set_cookie_header_string .= '; SameSite=None; Secure';

header($set_cookie_header_string);
?>
Set <?= $_GET["name"] ?>=<?= $_GET["value"] ?>
