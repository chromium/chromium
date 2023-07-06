<?php
header("HTTP/1.1 307 Temporary Redirect");
$mode = $_GET['mode'] ?? '';
if ($mode == "anonymous") {
    header("Access-Control-Allow-Origin: *");
} else if ($mode == "use-credentials") {
    header("Access-Control-Allow-Credentials: true");
    header("Access-Control-Allow-Origin: " . $_SERVER['HTTP_ORIGIN']);
}
header("Location: ".$_GET["url"]);
?>
