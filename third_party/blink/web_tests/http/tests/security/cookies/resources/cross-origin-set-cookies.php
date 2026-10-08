<?php
header('Content-Type: text/html; charset=UTF-8');
header('Set-Cookie: ' . $_GET['SetCookie']);
// Reflect the requesting origin so this works for both the HTTP and HTTPS
// origins used by web tests.
header('Access-Control-Allow-Origin: ' . ($_SERVER['HTTP_ORIGIN'] ?? '*'));
header('Access-Control-Allow-Credentials: true');
?>
