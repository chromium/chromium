function testJavascriptUrl(expectBlocked) {
  const iframe = document.getElementById('test-frame');

  const done = function() {
    iframe.removeEventListener('load', loadListener);
    clearTimeout(timeout);
    window.postMessage('next', '*');
  };

  // We need to use a timeout to detect iframe load failure since onload isn't
  // fired for a CSP violation on an iframe. Alternatively, we could have used
  // the 'securitypolicyviolation' event, however it is not supported for
  // violations in isolated worlds.
  const timeout = setTimeout(function() {
    // This means the iframe wasn;t loaded.
    if (expectBlocked) {
      console.log('PASS: Javascript url blocked as expected.');
    } else {
      console.log('FAIL: Javascript url blocked unexpectedly.');
    }
    done();
  }, 100);
  const loadListener = function(e) {
    if (expectBlocked) {
      console.log('FAIL: Javascript url worked unexpectedly.');
    } else {
      console.log('PASS: Javascript url worked as expected');
    }
    done();
  };

  iframe.addEventListener('load', loadListener);
  iframe.contentWindow.location.href =
      'javascript:alert(\'iframe javascript: src running\') || \'alerted\'';
}

const isolatedWorldId = 1;
const isolatedWorldSecurityOrigin = 'chrome-extensions://123';

function testJavascriptUrlInIsolatedWorld(expectBlocked) {
  const expectBlockedStr = expectBlocked ? 'true' : 'false';
  testRunner.evaluateScriptInIsolatedWorld(
      isolatedWorldId,
      String(eval('testJavascriptUrl')) +
          `\ntestJavascriptUrl(${expectBlockedStr});`);
}

const tests = [
  function() {
    console.log(
        'Testing main world. Javascript url should be blocked by main' +
        'world CSP.');
    testJavascriptUrl(true);
  },
  function() {
    console.log(
        'Testing isolated world with no csp. Javascript url should be' +
        ' blocked by main world CSP.');
    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    testJavascriptUrlInIsolatedWorld(true);
  },
  function() {
    console.log('Testing isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin,
        'script-src \'unsafe-inline\'');
    testJavascriptUrlInIsolatedWorld(false);
  },
  function() {
    console.log('Testing isolated world with strict csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'script-src \'none\'');
    testJavascriptUrlInIsolatedWorld(true);

    // Clear the isolated world data.
    testRunner.setIsolatedWorldInfo(1, null, null);
  },
];

// This test is meaningless without testRunner.
function setup() {
  let currentTest = 0;
  window.addEventListener('message', function(e) {
    if (e.data == 'next') {
      // Move to the next test.
      currentTest++;
      if (currentTest == tests.length) {
        testRunner.notifyDone();
        return;
      }

      // Move to the next sub-test.
      tests[currentTest]();
    }
  }, false);

  tests[0]();
}

if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
  window.addEventListener('load', setup);
}
