<?php
$http_origin = $_SERVER["HTTP_ORIGIN"] ?? null;
header("Set-Login: logged-in");
header("Access-Control-Allow-Credentials: true");
header("Access-Control-Allow-Origin: " . $http_origin);
?>
Header sent.
