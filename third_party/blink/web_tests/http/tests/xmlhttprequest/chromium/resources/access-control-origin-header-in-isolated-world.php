<?php
if ($_SERVER["HTTP_ORIGIN"] == "http://127.0.0.1:8000") {
  header("Access-Control-Allow-Origin: http://127.0.0.1:8000");

  if ($_SERVER["REQUEST_METHOD"] == "OPTIONS") {
    header("Access-Control-Allow-Headers: X-Custom-Header");
  } else {
    print $_SERVER["HTTP_X_CUSTOM_HEADER"];
  }
} else {
  header("HTTP/1.1 400");
}
?>
