<?php
header("Content-Type: font/woff2");
header("Access-Control-Allow-Origin: *");
if ($_SERVER['HTTP_REFERER'] != '') {
    $font = 'montez.woff2';
} else {
    $font = 'opensans.woff2';
}
echo file_get_contents($font);
?>
