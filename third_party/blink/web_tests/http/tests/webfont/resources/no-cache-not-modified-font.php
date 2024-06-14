<?php

if (isset($_SERVER['HTTP_IF_NONE_MATCH']) &&
    $_GET["tag"] == $_SERVER['HTTP_IF_NONE_MATCH']) {
  header('HTTP/1.0 304 Not Modified');
} else {
  header('HTTP/1.0 200 OK');
}

header("Cache-Control: no-cache");
header("Etag: {$_GET['tag']}");
header("Content-type: application/octet-stream");

$fp = fopen("../../resources/Ahem.ttf", "rb");
fpassthru($fp);

?>
