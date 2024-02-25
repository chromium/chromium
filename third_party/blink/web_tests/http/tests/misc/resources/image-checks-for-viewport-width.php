<?php
    $vw = $_SERVER["HTTP_VIEWPORT_WIDTH"] ?? null;
    $expected_vw = $_GET["viewport"] ?? null;

    if ((isset($expected_vw) && $vw == $expected_vw) || (isset($vw) && !isset($expected_vw))) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
