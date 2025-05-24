<?php
  header("Content-Type: application/json");
  header("Access-Control-Allow-Origin: *");

  if ($_GET["digest"]) {
    header("Unencoded-Digest: {$_GET["digest"]}");
  }
  if ($_GET["signature"]) {
    header("Signature: {$_GET["signature"]}");
  }
  if ($_GET["input"]) {
    header("Signature-Input: {$_GET["input"]}");
  }

  echo "{\"hello\": \"world\"}";
?>
