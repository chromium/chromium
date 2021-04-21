<?php
ob_start();
header("Content-Security-Policy: default-src 'self'; frame-src block-everything.com");
echo "<iframe src='http://127.0.0.1:8000/inspector-protocol/page/resources/csp.php'>";
