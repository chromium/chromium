function testEval(expectBlocked) {
  let evalBlocked;
  try {
    const x = eval('200');
    evalBlocked = (x != 200);
  } catch(e) {
    console.log(e);
    evalBlocked = true;
  }
  finally {
    if (expectBlocked === evalBlocked) {
      if (expectBlocked)
        console.log('PASS: eval blocked as expected.');
      else
        console.log('PASS: eval allowed as expected.');
    } else {
      if (expectBlocked)
        console.log('FAIL: eval allowed unexpectedly.');
      else
        console.log('FAIL: eval blocked unexpectedly.');
    }
    window.postMessage('next', '*');
  }
}

let isolatedWorldId = 1;
const isolatedWorldSecurityOrigin = 'chrome-extensions://123';

function testEvalInIsolatedWorld(expectBlocked) {
  const expectBlockedStr = expectBlocked ? 'true' : 'false';
  testRunner.evaluateScriptInIsolatedWorld(
      isolatedWorldId,
      String(testEval.toString()) + `\ntestEval(${expectBlockedStr});`);
}

const tests = [
  function() {
    console.log(
        'Testing main world. Eval should be blocked by main world CSP.');
    testEval(true);
  },
  function() {
    // TODO(karandeepb): Ideally we should use the main world CSP in this case.
    console.log(
        'Testing isolated world with no csp. Eval should be allowed.');
    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    testEvalInIsolatedWorld(false);

    // We use a different isolated world ID for each test since the eval-based
    // CSP checks are set-up when a v8::context is initialized. This happens for
    // an isolated world when a script is executed in it for the first time.
    isolatedWorldId++;
  },
  function() {
    console.log('Testing isolated world with strict csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin, 'script-src \'none\'');
    testEvalInIsolatedWorld(true);

    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    isolatedWorldId++;
  },
  function() {
    console.log('Testing isolated world with permissive csp.');
    testRunner.setIsolatedWorldInfo(
        isolatedWorldId, isolatedWorldSecurityOrigin,
        'script-src \'unsafe-eval\'');
    testEvalInIsolatedWorld(false);

    testRunner.setIsolatedWorldInfo(isolatedWorldId, null, null);
    isolatedWorldId++;
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
