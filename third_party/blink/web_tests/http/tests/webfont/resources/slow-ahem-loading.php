<?php
// Extra query parameters such as `t` are intentionally ignored so callers can
// construct distinct URLs per test case and prevent request coalescing.
$delay = isset($_GET['delay']) ? intval($_GET['delay']) : 1000;
$delay = max(0, min(60000, $delay));
header("Content-Type: application/octet-stream");
header("Cache-Control: no-cache, no-store");
if ($delay > 0) {
    usleep($delay * 1000);
}
readfile(__DIR__ . "/../../resources/Ahem.ttf");
