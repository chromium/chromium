<?php
if (isset($_GET['Redirect'])) {
  header('HTTP/1.0 302 Found');
  header('Location: ./navigation-preload-redirected.html');
} if (isset($_GET['BrokenChunked'])) {
  header("Content-type: text/html; charset=UTF-8");
  header("Transfer-encoding: chunked");
  echo "hello\nworld\n";
} else {
  header("Content-Type: text/html; charset=UTF-8");
  echo "OK";
}
?>
