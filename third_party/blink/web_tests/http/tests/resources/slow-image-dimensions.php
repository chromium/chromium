<?php
$name = $_GET['name'];
$mimeType = $_GET['mimeType'];
$sleepTime = $_GET['sleep'];
$size = filesize($name);

usleep($sleepTime*1000);

header('Content-Type: ' . $mimeType);
header('Content-Length: ' . $size);
if (isset($_GET['expires']))
  header('Cache-control: max-age=0');
else
  header('Cache-control: max-age=86400');

$output = fopen("php://output", 'r+');
$file = fopen($name, "rb");
$buffer = fread($file, 16);
fwrite($output, $buffer, 16);
if (ob_get_level() > 0)
  ob_flush();
flush();

usleep($sleepTime*1000);

$buffer = fread($file, $size - 16);
fwrite($output, $buffer, $size - 16);
