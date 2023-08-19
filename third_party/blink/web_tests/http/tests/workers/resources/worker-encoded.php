<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

// Parent document has Windows-1251 encoding. This test verifies that worker script gets decoded using
// parent document encoding or the one in http header, if specified.
$charset=$_GET['charset'] ?? null;
if ($charset == "koi8-r") {
    header("Content-Type: text/javascript;charset=koi8-r");
    print("postMessage('Has http header with charset=koi8-r');");
} else {
    header("Content-Type: text/javascript");
    print("postMessage('Has no http header with charset');");
}

print("postMessage('Original test string: ' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442));");
print("postMessage('Test string encoded using koi8-r: \xF0\xD2\xC9\xD7\xC5\xD4.');");
print("postMessage('Test string encoded using Windows-1251: \xCF\xF0\xE8\xE2\xE5\xF2.');");
print("postMessage('Test string encoded using UTF-8: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82.');");

// Test how XHR decodes its response text. Should be UTF8 or a charset from http header.
print("var xhr = new XMLHttpRequest(); xhr.open('GET', 'xhr-response.php', false);");
print("xhr.send(); postMessage(xhr.responseText);");

print("var xhr = new XMLHttpRequest(); xhr.open('GET', 'xhr-response.php?charset=koi8-r', false);");
print("xhr.send(); postMessage(xhr.responseText);");

// Test that URL completion is done using UTF-8, regardless of the worker's script encoding.
// The server script verifies that query parameter is encoded in UTF-8.
print("var xhr = new XMLHttpRequest(); xhr.open('GET', 'xhr-query-utf8.php?query=' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442), false);");
print("xhr.send(); postMessage(xhr.responseText);");

print("importScripts('subworker-encoded.php');");

print("postMessage('exit');");
?>
