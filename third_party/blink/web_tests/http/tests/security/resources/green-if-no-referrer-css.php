<?php
header('Content-Type: text/css');
$HTTP_REFERER = $_SERVER['HTTP_REFERER'] ?? null;
if ($HTTP_REFERER == '') {
    echo "body { background-color: green; }";
} else {
    echo "body { background-color: red; }";
}
?>
