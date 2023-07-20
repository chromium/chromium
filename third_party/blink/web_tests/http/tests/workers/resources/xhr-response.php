<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

// The XHR response contains word 'Привет!' (Hello!), in koi8-r encoding (with http header) or in utf8.
// It should always be decoded as 'Привет!', regardless of the parent document/worker encoding.
$charset=$_GET['charset'] ?? null;
if ($charset == "koi8-r") {
    header("Content-Type: text/plain; charset=koi8-r");
    print("XHR: \xF0\xD2\xC9\xD7\xC5\xD4");
} else {
    header("Content-Type: text/plain;");
    print("XHR: Привет");
}
?>
