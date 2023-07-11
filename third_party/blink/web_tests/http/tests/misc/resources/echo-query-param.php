<?php
header("Content-Type: text/html; charset=UTF-8");
echo "<html><body><div id='output'>";
$q = $_GET['q'] ?? null;
echo $q;
echo "</div></body></html>";
?>
