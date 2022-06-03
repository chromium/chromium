<?php
    header("Link: <http://127.0.0.1:8000/resources/square.png?nonzero>;rel=preload;as=image;media=\"(min-width: 1px)\"", false);
    header("Link: <http://127.0.0.1:8000/resources/square.png?zero>;rel=preload;as=image;media=\"(width: 0px)\"", false);
?>
<!DOCTYPE html>
<meta name="viewport" content="width=device-width">
<script>
    window.addEventListener("load", function() {
        var entries = performance.getEntriesByType("resource");
        var nonzeroLoaded = false;
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].name.indexOf("?zero") != -1)
                window.opener.postMessage("zeroloaded", "*");
            if (entries[i].name.indexOf("?nonzero") != -1)
                nonzeroLoaded = true;
        }
        if (nonzeroLoaded)
            window.opener.postMessage("success", "*")
        else
            window.opener.postMessage("nonzeronotloaded", "*");
    });
</script>
<script src="/resources/slow-script.pl?delay=200"></script>

