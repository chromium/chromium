<?php
$allowOrigin = $_GET['allow'];
if ($allowOrigin == "true") {
    header("Access-Control-Allow-Origin: *");
}

if (array_key_exists('tao', $_GET)) {
    header("Timing-Allow-Origin: " . $_GET['tao']);
}

$file = $_GET['file'];
$fp = fopen($file, 'rb');
header("Content-Type: image/png");
header("Content-Length: " . filesize($file));

fpassthru($fp);
exit;
?>
