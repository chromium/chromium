<?php
    $max_age = 12 * 31 * 24 * 60 * 60; //one year
    $expires = gmdate(DATE_RFC1123, time() + $max_age);
    // Dec 01, 1989, 8:00:00AM.
    $last_modified = gmdate(DATE_RFC1123, 628502400);

    header("Cache-Control: public, max-age=" . 5*$max_age);
    header("Cache-control: max-age=0");
    header("Expires: " . $expires);
    header("Content-Type: text/javascript");
    header("Last-Modified: " . $last_modified);

    echo("console.log(\"Done.\");");
?>
