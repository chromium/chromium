<?php
  header("Content-Type: text/javascript");
  sleep($_GET["delay"]);
  echo "log('" . $_GET["msg"] . "');\n";
  if (($_GET["done"] ?? "") == "1") {
      echo "if (window.testRunner)\n";
      echo "    testRunner.notifyDone();\n";
  }
?>
