<?php
    header("ACCEPT-CH: DPR, Width, Viewport-Width");
?>
<!DOCTYPE html>
<body>
    <script>
        var fail = function(num) {
            return function() {
                parent.postMessage("fail "+ num, "*");
            }
        };

        var success = function() {
            parent.postMessage("success", "*");
        };

        var loadRWImage = function() {
            var img = new Image();
            img.src = 'resources/image-checks-for-width.php';
            img.sizes = '500';
            img.onload = fail(3);
            img.onerror = success;
            document.body.appendChild(img);
        };
        var loadViewportImage = function() {
            var img = new Image();
            img.src = 'resources/image-checks-for-viewport-width.php';
            img.onload = fail(2);
            img.onerror = loadRWImage;
            document.body.appendChild(img);
        };
        var img = new Image();
        img.src = 'resources/image-checks-for-dpr.php';
        img.onload = fail(1);
        img.onerror = loadViewportImage;
        document.body.appendChild(img);
    </script>
</body>
