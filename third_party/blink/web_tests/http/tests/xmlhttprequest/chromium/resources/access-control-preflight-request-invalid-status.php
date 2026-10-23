<?php
    $code = $_GET['code'];
    $request_origin_value = $_SERVER["HTTP_ORIGIN"] ?? null;
    $access_control_request_headers =
        $_SERVER["HTTP_ACCESS_CONTROL_REQUEST_HEADERS"] ?? null;

    if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
        if ($code == "400") {
            header("HTTP/1.1 400");
        }
        else if ($code == "501"){
            header("HTTP/1.1 501");
        }
        else if ($code == "301"){
            header("HTTP/1.1 301");
        }
        header("Access-Control-Allow-Methods: GET");
        header("Access-Control-Max-Age: 1");
    } else {
        header("HTTP/1.1 200");
    }
    if (!is_null($request_origin_value)) {
        header("Access-Control-Allow-Origin: $request_origin_value");
    }
    if (!is_null($access_control_request_headers))
        header("Access-Control-Allow-Headers: $access_control_request_headers");
?>
