<?php
$status = (int)$_REQUEST['status'];
$redirected = $_GET['redirected'] ?? null;
if ($status > 200 && !$redirected) {
  header("Location: redirect-methods-result.php?redirected=true", TRUE, $status);
  exit();
}
?>
Request Method: <?php echo $_SERVER['REQUEST_METHOD'] ?><br> 
Request Body: <?php echo @file_get_contents('php://input') ?><br>
Request Content-Type: <?php echo $_SERVER["CONTENT_TYPE"]; ?>
