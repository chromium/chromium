<?php
$sleepTime = $_GET['sleep'];

usleep($sleepTime*1000);

if (isset($_GET['redirect_name'])) {
    header('HTTP/1.1 302');
    header('Location: ' . $_GET['redirect_name']);
    exit;
}

$name = $_GET['name'];
$mimeType = $_GET['mimeType'] ?? '';

header('Content-Type: ' . $mimeType);
header('Content-Length: ' . filesize($name));
if (isset($_GET['expires']))
  header('Cache-control: max-age=0'); 
else
  header('Cache-control: max-age=86400'); 

readfile($name);
