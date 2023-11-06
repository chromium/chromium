<?php
// Redirects back to the first party site.
header('HTTP/1.1 307 Temporary Redirect');
header('Expires: Thu, 01 Dec 2003 16:00:00 GMT');
header('Cache-Control: no-cache, must-revalidate');
header('Pragma: no-cache');
header('Location: https://firstparty.test:8443/inspector-protocol/resources/redirect-chain.html?end');
?>
