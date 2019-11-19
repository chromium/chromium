function testSendBeacon() {
  navigator.sendBeacon(
      'http://localhost:8000/security/isolatedWorld/resources/empty.html',
      'data');
  window.postMessage('next', '*');
}

const isolatedWorldId = 1;
const isolatedWorldSecurityOrigin = 'chrome-extensions://123';

const tests = [
  function() {
    console.log(
        'Testing main world. Request should be blocked by main world CSP.');
    testSendBeacon();
  },
  function() {
    console.log(
        'Testing isolated world with no csp. Request should be blocked by main world CSP.');
    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    testRunner.evaluateScriptInIsolatedWorld(
        isolatedWorldId,
        String(eval('testSendBeacon')) + '\ntestSendBeacon();');
  },
  function() {
    console.log('Testing isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src *');
    testRunner.evaluateScriptInIsolatedWorld(
        isolatedWorldId,
        String(eval('testSendBeacon')) + '\ntestSendBeacon();');
  },
  function() {
    console.log('Testing isolated world with strict csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'connect-src \'self\'');
    testRunner.evaluateScriptInIsolatedWorld(
        isolatedWorldId,
        String(eval('testSendBeacon')) + '\ntestSendBeacon();');

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
