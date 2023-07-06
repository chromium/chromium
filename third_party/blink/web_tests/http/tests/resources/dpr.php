<?php
$name = $_GET['name'];
$mimeType = $_GET['mimeType'];
$dpr = $_GET['dpr'];
$expires = $_GET['expires'] ?? null;
$HTTP_IF_NONE_MATCH = $_SERVER['HTTP_IF_NONE_MATCH'] ?? '';

header('Content-Type: ' . $mimeType);
header('Content-Length: ' . filesize($name));
if ($expires)
  header('Cache-control: max-age=0'); 
else
  header('Cache-control: max-age=86400'); 
header('ETag: dprimage'); 

if ($HTTP_IF_NONE_MATCH == 'dprimage') {
  header('HTTP/1.1 304 Not Modified');
  exit;
}

header('Content-DPR: '. $dpr);

readfile($name);
