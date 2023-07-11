<?php
    $boundary = "cutHere";

    function sendPart($data)
    {
        global $boundary;

        echo("Content-Type: image/png\r\n\r\n");
        echo($data);
        echo("--$boundary\r\n");
        flush();
    }

    $i = 1;
    while (isset($_GET['img' . $i])) {
        $img = $_GET['img' . $i];
        $images[$i] = file_get_contents($img);
        $i++;
    }

    if (isset($_GET['interval']))
        $interval = $_GET['interval'] * 1000000;
    else
        $interval = 1000000;

    header("Content-Type: multipart/x-mixed-replace; boundary=$boundary");

    echo("--$boundary\r\n");

    ob_end_flush();
    do {
        for ($k = 1; $k < $i; $k++) {
            sendPart($images[$k]);
            usleep($interval);
        }
    } while ($_GET['loop']);

    if (isset($_GET['wait']))
        usleep($_GET['wait'] * 1000000);
?>
