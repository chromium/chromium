<?php

    $origin = $_GET["origin"] ?? null;
    $credentials = $_GET["credentials"] ?? null;

    if ($origin)
        header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    if ($credentials)
        header("Access-Control-Allow-Credentials: true");
    
    $name = 'captions.vtt';
    $fp = fopen($name, 'rb');
    header("Content-Type: text/vtt");
    header("Content-Length: " . filesize($name));
    
    fpassthru($fp);
    exit;
?>
