<?php
ob_start();
header('Access-Control-Allow-Origin: *');
header('Content-Type: text/css');
if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS')
  exit;
$url = $_GET["url"];
$trimmed_url = $url;
$last_query = strrpos($url, "?");
if ($last_query)
  $trimmed_url = substr($url, 0, $last_query);
echo file_get_contents($trimmed_url);
?>
