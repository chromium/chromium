<?php
// Generate token with the command:
// generate_token.py http://127.0.0.1:8000 CookieStore --expire-timestamp=2000000000
header("Origin-Trial: AuCNc4F6ez8bdiKV6reoNKgzu2afmtUl5FgKkP6jdrbbCqVh8BfddejNqciWMz+V+oZXxJdW1LU5nQuC0Ij2GQkAAABTeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQ29va2llU3RvcmUiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
header('Content-Type: application/javascript');
?>
importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_properties_exist(this, {
     'CookieStore': ['get', 'getAll', 'set', 'delete', 'subscribeToChanges',
                     'getChangeSubscriptions'],
     'ExtendableCookieChangeEvent': ['changed', 'deleted'],
  });
}, 'Cookie Store API interfaces and properties in Origin-Trial enabled serviceworker.');

test(t => {
  assert_true('cookieStore' in self, 'cookieStore property exists on global');
  assert_true('oncookiechange' in self,
              'oncookiechange property exists on global');
}, 'Cookie Store API entry points in Origin-Trial enabled serviceworker.');

done();
