<?php
header('Content-Type: image/png');
$HTTP_REFERRER = $_SERVER['HTTP_REFERER'] ?? null;
if ($HTTP_REFERRER != '') {
    $img = 'red200x100.png';
} else {
    $img = 'green250x50.png';
}
echo file_get_contents($img);
?>
