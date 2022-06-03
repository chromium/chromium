<?php
    $filename = 'square20.png';
    $fp = fopen($filename, 'r');
    header("Content-Type: image/png");
    header("Content-Length: " . filesize($filename));
    header("Access-Control-Allow-Origin: *");
    header("Timing-Allow-Origin: *");
    echo fread($fp, filesize($filename));
    fclose($fp)
?>
