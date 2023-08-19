<?php
    $sleep = $_GET['delay'] ?? null;
    if (isset($sleep))
        usleep($sleep*1000);
    $url = $_GET['url'];
    $refresh = $_GET['refresh'] ?? null;

    if (isset($refresh)) {
        header("HTTP/1.1 200");
        header("Refresh: $refresh; url=$url");
        return;
    }

    header("Location: $url");
    if (isset($_GET['cors_allow_origin']))
        header("Access-Control-Allow-Origin: " . $_GET['cors_allow_origin']);
    if (isset($_GET['timing_allow_origin']))
        header("Timing-Allow-Origin: " . $_GET['timing_allow_origin']);

    $code = $_GET['code'] ?? null;
    if (!isset($code))
        $code = 302;
    header("HTTP/1.1 $code");
?>
