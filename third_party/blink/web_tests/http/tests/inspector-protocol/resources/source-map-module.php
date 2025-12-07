<?php
header("Content-Type: application/javascript");
header("SourceMap: http://localhost/source.js.map");
?>

export function greeter() {
  console.log('Hello with a SourceMap header!');
}
