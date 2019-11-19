<?php
/* This is copied from WPT's resources/service-worker-csp-worker.py in
   https://crrev.com/c/613001. */

$bodyDefault = <<<EOL
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('/resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTPS_REMOTE_ORIGIN +
        base_path() + 'empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for default-src');

test(function() {
    assert_throws(EvalError(),
                  function() { eval('1 + 1'); },
                  'eval() should throw EvalError.')
    assert_throws(EvalError(),
                  function() { new Function('1 + 1'); },
                  'new Function() should throw EvalError.')
  }, 'eval test for default-src');

async_test(function(t) {
    fetch(host_info.HTTPS_REMOTE_ORIGIN +
          base_path() + 'fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for default-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      base_path() + 'redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTPS_REMOTE_ORIGIN +
                         base_path() + 'fetch-access-control.php?';
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('Redirected fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for default-src');
EOL;

$bodyScript = <<<EOL
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('/resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTPS_REMOTE_ORIGIN +
        base_path() + 'empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for script-src');

test(function() {
    assert_throws(EvalError(),
                  function() { eval('1 + 1'); },
                  'eval() should throw EvalError.')
    assert_throws(EvalError(),
                  function() { new Function('1 + 1'); },
                  'new Function() should throw EvalError.')
  }, 'eval test for script-src');

async_test(function(t) {
    fetch(host_info.HTTPS_REMOTE_ORIGIN +
          base_path() + 'fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          t.done();
        }, function(){
          assert_unreached('fetch should not fail.');
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for script-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      base_path() + 'redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTPS_REMOTE_ORIGIN +
                         base_path() + 'fetch-access-control.php?';
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          t.done();
        }, function(e){
          console.error(e);
          assert_unreached('Redirected fetch should not fail.');
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for script-src');
EOL;

$bodyConnect = <<<EOL
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('/resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTPS_REMOTE_ORIGIN +
        base_path() + 'empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_false(import_script_failed,
                 'Importing the other origins script should not fail.');
  }, 'importScripts test for connect-src');

test(function() {
    var eval_failed = false;
    try {
      eval('1 + 1');
      new Function('1 + 1');
    } catch(e) {
      eval_failed = true;
    }
    assert_false(eval_failed,
                 'connect-src without unsafe-eval should not block eval().');
  }, 'eval test for connect-src');

async_test(function(t) {
    fetch(host_info.HTTPS_REMOTE_ORIGIN +
          base_path() + 'fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for connect-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      base_path() + 'redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTPS_REMOTE_ORIGIN +
                         base_path() + 'fetch-access-control.php?';
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('Redirected fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for connect-src');
EOL;


header('Content-Type: application/javascript');

$body = 'ERROR: Unknown directive';
if (isset($_GET['directive'])) {
  switch($_GET['directive']) {
    case 'default':
      header("Content-Security-Policy: default-src 'self'");
      $body = $bodyDefault;
      break;
    case 'script':
      header("Content-Security-Policy: script-src 'self'");
      $body = $bodyScript;
      break;
    case 'connect':
      header("Content-Security-Policy: connect-src 'self'");
      $body = $bodyConnect;
      break;
  }
}

echo $body;
