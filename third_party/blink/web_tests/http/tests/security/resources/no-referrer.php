<?php
header("Cache: no-cache, no-store");

$refer = $_SERVER['HTTP_REFERER'] ?? null;
if ($refer && $refer != "")
    print("log('External script (HTTP Referer): FAIL');\n");
else
    print("log('External script (HTTP Referer): PASS');\n");
?>
if (document.referrer.toString() != "")
    log('External script (JavaScript): FAIL');
else
    log('External script (JavaScript): PASS');
