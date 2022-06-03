<?php
// Generate token with the command:
// generate_token.py http://127.0.0.1:8000 BadgingV2 --expire-timestamp=2000000000
header("Origin-Trial: AqzH1yAjqt/6grJkR3r1584FLOYa+kkfoenZBdnmBOShEN/eGrOF7OoxdPXg5e2b+KeB+ysH8qp/F9eyimHZygIAAABReyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQmFkZ2luZ1YyIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
header('Content-Type: application/javascript');
?>
'use strict';

importScripts('/resources/testharness.js');

function assert_function_on(object, function_name, explanation) {
  assert_equals(typeof object[function_name], 'function', explanation);
}

test(t => {
  assert_function_on(navigator, 'setAppBadge', 'setAppBadge is not defined on navigator');
  assert_function_on(navigator, 'clearAppBadge', 'clearAppBadge is not defined on navigator');
}, 'Badge API interfaces and properties in Origin-Trial enabled service worker.');

done();
