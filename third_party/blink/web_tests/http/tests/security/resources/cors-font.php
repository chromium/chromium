<?php

$font = "../../resources/Ahem.ttf";

header("Cache-Control: public, max-age=86400");
header('Last-Modified: ' . gmdate("D, d M Y H:i:s", filemtime($font)) . " GMT");
header("Content-Type: font/truetype");
header("Content-Length: " . filesize($font));

$cors_arg = strtolower($_GET["cors"]);
if ($cors_arg != "false") {
    if ($cors_arg == "" || $cors_arg == "true") {
        header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    } else {
        header("Access-Control-Allow-Origin: " . $cors_arg . "");
    }
}
if (strtolower($_GET["credentials"]) == "true") {
    header("Access-Control-Allow-Credentials: true");
}

header("Timing-Allow-Origin: *");
ob_clean();
flush();
readfile($font);

?>
