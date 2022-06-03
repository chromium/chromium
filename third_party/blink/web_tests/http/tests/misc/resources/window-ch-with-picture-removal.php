<?php
    header("ACCEPT-CH: DPR, Width, Viewport-Width");
?>
<!DOCTYPE html>
<body>
<script>
    window.addEventListener("message", function (message) {
        var pic = document.getElementById("pic");
        pic.removeChild(document.getElementById("firstsource"));
        setTimeout(function(){fail(4);}, 200);
    });

    var fail = function(num) {
        opener.postMessage("fail "+ num, "*");
    };

    var success = function() {
        opener.postMessage("success", "*");
    };

    var remove = function() {
        opener.postMessage("remove", "*");
    };

    var counter = 1;
    var error = function() {
        fail(counter);
    }
    var load = function() {
        if (counter == 1) {
            ++counter;
            remove();
            return;
        }
        success();
    }
</script>
<picture id=pic>
    <source sizes="50vw" id="firstsource" media="(min-width: 800px)" srcset="image-checks-for-width.php?rw=400">
    <source sizes="40vw" srcset="image-checks-for-width.php?rw=320">
    <img onerror="error()" onload="load()">
</picture>
