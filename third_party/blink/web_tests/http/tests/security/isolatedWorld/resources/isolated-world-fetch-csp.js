function testFetch(expectBlocked, redirect) {
  let url =
      'http://127.0.0.1:8000/security/isolatedWorld/resources/access_control_allow_origin.php';

  if (redirect) {
    url = `http://127.0.0.1:8000/resources/redirect.php?url=${
        url}&cors_allow_origin=*&delay=100`;
  }

  fetch(url)
      .then(function(response) {
        return response.text();
      })
      .then(function(responseText) {
        const success = responseText == 'Hello world';
        if (expectBlocked) {
          console.log(
              'FAIL: Request succeeded unexpectedly with response ' +
              responseText);
        } else if (!success) {
          console.log(
              'FAIL: Request succeeded with incorrect response ' +
              responseText);
        } else {
          console.log('PASS: Request succeeded as expected.');
        }
      })
      .catch(function(error) {
        if (expectBlocked)
          console.log('PASS: Request blocked by CSP as expected.');
        else
          console.log('FAIL: Request failed unexpectedly.');
      })
      .finally(function() {
        window.postMessage('next', '*');
      });
}

const isolatedWorldId = 1;
const isolatedWorldSecurityOrigin = 'chrome-extensions://123';

function testFetchInIsolatedWorld(expectBlocked, redirect) {
  const expectBlockedStr = expectBlocked ? 'true' : 'false';
  const redirectStr = redirect ? 'true' : 'false';
  testRunner.evaluateScriptInIsolatedWorld(
      isolatedWorldId,
      String(eval('testFetch')) +
          `\ntestFetch(${expectBlockedStr}, ${redirectStr});`);
}

const tests = [
  function() {
    console.log(
        'Testing main world. Request should be blocked by main world CSP.');
    testFetch(true);
  },
  function() {
    console.log(
        'Testing isolated world with no csp. Request should be blocked by ' +
        'main world CSP.');
    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    testFetchInIsolatedWorld(true);
  },
  function() {
    console.log('Testing isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src *');
    testFetchInIsolatedWorld(false);
  },
  function() {
    console.log(
        'Testing fetch redirect in isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src *');
    testFetchInIsolatedWorld(false, true /* redirect */);
  },
  function() {
    console.log('Testing isolated world with strict csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src \'self\'');
    testFetchInIsolatedWorld(true);

    // Clear the isolated world data.
    testRunner.setIsolatedWorldInfo(1, null, null);
  },
  function() {
    console.log('Testing fetch redirect in isolated world with strict csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src \'self\'');
    testFetchInIsolatedWorld(true, true /* redirect */);
  },
];

// This test is meaningless without testRunner.
if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();

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
