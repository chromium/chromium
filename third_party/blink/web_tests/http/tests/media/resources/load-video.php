<?php

    $fileName = $_GET["name"] ?? null;
    $type = $_GET["type"] ?? null;
    $norange = $_GET["norange"] ?? null;
    $cors_allow_origin = $_GET["cors_allow_origin"] ?? null;

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    $_GET['norange'] = $norange;
    $_GET['cors_allow_origin'] = $cors_allow_origin;
    @include("./serve-video.php");

?>
