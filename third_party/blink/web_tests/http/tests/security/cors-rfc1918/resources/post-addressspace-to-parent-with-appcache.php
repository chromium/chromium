<?php
  if (isset($_GET["csp"]))
    header("Content-Security-Policy: treat-as-public-address");
?>
<html manifest="/security/cors-rfc1918/resources/appcache.php">
<script>
    window.applicationCache.oncached = window.applicationCache.onnoupdate = function (e) {
        window.parent.postMessage({
            "origin": window.location.origin,
            "addressSpace": document.addressSpace
        }, "*");
    }
</script>
