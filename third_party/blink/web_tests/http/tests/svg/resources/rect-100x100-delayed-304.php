<?php
if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"])) {
  usleep(20000);
  header("HTTP/1.0 304 Not Modified");
  exit();
}

header('Cache-Control: max-age=0, must-revalidate');
header('Content-Type: image/svg+xml');
header('Last-Modified: Mon, 31 Jul 2017 23:59:59 GMT');
echo(file_get_contents("rect-100x100.svg"));
?>
