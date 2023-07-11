<?php

    $auth = $_SERVER["HTTP_AUTHORIZATION"] ?? null;
    $url = $_SERVER["REQUEST_URI"];

    if (isset($auth) || stripos($url, "user:pass") !== false)
       die;

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php");

?>
