<?php
  header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
  header("Cache-Control: no-cache, must-revalidate");
  header("Pragma: no-cache");
  if ($_GET["csp"]) {
    $csp = $_GET["csp"];
    // If the magic quotes option is enabled, the CSP could be escaped and
    // the test would fail.
    if (get_magic_quotes_gpc()) {
      $csp = stripslashes($csp);
    }
    header("Content-Security-Policy: " . $csp);
  } else if ($_GET["type"] == "multiple-headers") {
    header("Content-Security-Policy: connect-src 'none'");
    header("Content-Security-Policy: script-src 'self'", false);
  }

  if ($_GET["expectation"] != "none") {
?>
importScripts("/resources/testharness.js");
<?php
  }

  if ($_GET["type"] == "eval") {
    if ($_GET["expectation"] == "none") {
?>
try {
    eval("1+1");
    postMessage({"state": "allowed", "msg": "`eval()` executed with '<?php echo $_GET["csp"] ?>'"});
} catch (e) {
    postMessage({"state": "blocked", "msg": "`eval()` threw '" + e.name + "' with '<?php echo $_GET["csp"] ?>'"});
}
<?php
    } else if ($_GET["expectation"] == "blocked") {
?>
test(function (t) {
    assert_throws_js(EvalError,
                     function () { eval("1 + 1"); },
                     "`eval()` should throw 'EvalError'.");

    assert_throws_js(EvalError,
                     function () { var x = new Function("1 + 1"); },
                     "`new Function()` should throw 'EvalError'.");

    assert_equals(setTimeout("assert_unreached('setTimeout([string]) should not execute.')", 0), 0, "`setTimeout([string])` should return 0.");
}, "`eval()` with '<?php echo $_GET["csp"] ?>' blocked");
<?php
    } else {
?>
importScripts("/resources/testharness.js");
test(function (t) {
    var x = 0;
    x = eval("1 + 1");
    assert_equals(x, 2);
}, "`eval()` with '<?php echo $_GET["csp"] ?>' allowed");
<?php
    }
  }

  if ($_GET["expectation"] != "none") {
?>
// An explicit `done()` is required for Workers.
done();
<?php
  }
?>
