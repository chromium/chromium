<?php
  header("Content-Type: application/json");
  header("Access-Control-Allow-Origin: *");

  if (!empty($_GET["digest"])) {
    header("Unencoded-Digest: {$_GET["digest"]}");
  }
  if (!empty($_GET["signature"])) {
    header("Signature: {$_GET["signature"]}");
  }
  if (!empty($_GET["input"])) {
    header("Signature-Input: {$_GET["input"]}");
  }

  echo "{\"hello\": \"world\"}";
?>
