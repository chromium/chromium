<?php
header('Content-Type:application/javascript');
$query_string = $_SERVER['QUERY_STRING'] ?? null;
switch ($query_string) {
  case 'parse-error':
    echo 'var foo = function() {;';
    exit;
  case 'undefined-error':
    echo 'foo.bar = 42;';
    exit;
  case 'uncaught-exception':
    echo 'throw new Error;';
    exit;
  case 'caught-exception':
    echo 'try { throw new Error; } catch(e) {}';
    exit;
  case 'import-malformed-script':
    echo 'importScripts("malformed-worker.php?parse-error");';
    exit;
  case 'import-no-such-script':
    echo 'importScripts("no-such-script.js");';
    exit;
}
?>
