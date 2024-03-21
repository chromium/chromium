<?php
header("Content-Type: application/json");
if (isset($_GET['origin'])) {
  header("Access-Control-Allow-Origin: " . $_GET['origin']);
  header("Access-Control-Allow-Credentials: true");
}
?>
{
  "token": "<?php echo $_POST["account_id"]; ?>"
}
