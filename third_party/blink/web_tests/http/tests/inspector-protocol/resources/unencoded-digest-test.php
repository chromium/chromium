<?php
  header("Content-Type: application/json");
  header("Access-Control-Allow-Origin: *");

  if ($_GET["digest"]) {
    header("Unencoded-Digest: {$_GET["digest"]}");
  }

  echo "{\"hello\": \"world\"}";
?>
