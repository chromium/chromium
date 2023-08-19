<?php
header("Content-Type: font/woff2");
header("Access-Control-Allow-Origin: *");
$HTTP_REFERER = $_SERVER['HTTP_REFERER'] ?? null;
if ($HTTP_REFERER != '') {
    $font = 'montez.woff2';
} else {
    $font = 'opensans.woff2';
}
echo file_get_contents($font);
?>
