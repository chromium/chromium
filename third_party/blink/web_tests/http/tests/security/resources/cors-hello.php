<?php
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
header("Timing-Allow-Origin: http://127.0.0.1:8000");
?>
<!DOCTYPE html>
<html>
<head></head>
<body><h1>Hello</h1></body>
</html>
