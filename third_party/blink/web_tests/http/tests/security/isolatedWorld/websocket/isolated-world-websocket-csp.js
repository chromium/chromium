function testWebSocket(expectBlocked) {
  const ws = new WebSocket('ws://127.0.0.1:8880/echo');
  ws.onopen = function() {
    ws.send('data');
  };
  ws.onmessage = function(msg) {
    if (expectBlocked)
      console.log('FAIL: Request succeeded unexpectedly.');
    else if (msg.data != 'data')
      console.log('FAIL: Invalid message received ' + JSON.stringify(msg));
    else
      console.log('PASS: Request succeeded as expected.');

    window.postMessage('next', '*');
  };
  ws.onerror = function(error) {
    if (expectBlocked)
      console.log('PASS: Request blocked by CSP as expected.');
    else
      console.log('FAIL: Request failed unexpectedly.');

    window.postMessage('next', '*');
  };
}

const isolatedWorldId = 1;
const isolatedWorldSecurityOrigin = 'chrome-extensions://123';

function testWebSocketInIsolatedWorld(expectBlocked) {
  const expectBlockedStr = expectBlocked ? 'true' : 'false';
  testRunner.evaluateScriptInIsolatedWorld(
      isolatedWorldId,
      String(eval('testWebSocket')) + `\ntestWebSocket(${expectBlockedStr});`);
}

const tests = [
  function() {
    console.log(
        'Testing main world. Request should be blocked by main world CSP.');
    testWebSocket(true);
  },
  function() {
    console.log(
        'Testing isolated world with no csp. Request should be blocked by ' +
        'main world CSP.');
    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    testWebSocketInIsolatedWorld(true);
  },
  function() {
    console.log('Testing isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src *');
    testWebSocketInIsolatedWorld(false);
  },
  function() {
    console.log('Testing isolated world with strict csp.');
    console.log(
        'internals.runtimeFlags.isolatedWorldCSPEnabled is ' +
        internals.runtimeFlags.isolatedWorldCSPEnabled);
    const expectBlocked = internals.runtimeFlags.isolatedWorldCSPEnabled;
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src \'none\'');
    testWebSocketInIsolatedWorld(expectBlocked);

    // Clear the isolated world data.
    testRunner.setIsolatedWorldInfo(1, null, null);
  },
];

// This test is meaningless without testRunner.
if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
  testRunner.dumpPingLoaderCallbacks();

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
