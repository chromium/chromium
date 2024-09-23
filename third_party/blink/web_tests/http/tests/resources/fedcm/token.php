<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: " . $_SERVER["HTTP_ORIGIN"]);
header("Access-Control-Allow-Credentials: true");
?>
{
  "token": "<?php echo $_POST["account_id"]; ?>"
}
