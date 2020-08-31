<?php
header('Content-Type: text/plain');
if (isset($_SERVER['HTTP_SEC_BFCACHE_EXPERIMENT']))
    echo 'Sec-bfcache-experiment: ' . $_SERVER['HTTP_SEC_BFCACHE_EXPERIMENT'];
else
    echo 'Sec-bfcache-experiment: <missing>';
?>
