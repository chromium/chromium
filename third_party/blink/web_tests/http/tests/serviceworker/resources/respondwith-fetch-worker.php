<?php
header("Content-Type: text/javascript");
header("Service-Worker-Allowed: /serviceworker");

echo 'addEventListener("fetch", e => e.respondWith(fetch(e.request)));';
?>
