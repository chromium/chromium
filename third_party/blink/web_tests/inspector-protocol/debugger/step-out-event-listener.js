// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      '<div id="myDiv"></div>',
      'Tests that stepping out of an event listener will lead to a pause in the next event listener.');

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  const scripts = new Map();
  dp.Debugger.onScriptParsed(event => {
    scripts.set(event.params.scriptId, event.params.url);
  });

  await session.evaluate(`function testFunction()
{
    function inner()
    {
        var div = document.getElementById("myDiv");
        function fooEventHandler1()
        {
            div.textContent += "Recieved foo event(1)!\\n";
        }

        function fooEventHandler2()
        {
            div.textContent += "Recieved foo event(2)!\\n";
        }

        div.addEventListener("foo", fooEventHandler1);
        div.addEventListener("foo", fooEventHandler2);

        var e = new CustomEvent("foo");
        debugger;
        div.dispatchEvent(e);

        div.removeEventListener("foo", fooEventHandler1);
        div.removeEventListener("foo", fooEventHandler2);
    }
    inner();
}
//# sourceURL=test.js`);

  function printCallFrames(pausedEvent) {
    testRunner.log(`Call stack (reason: ${pausedEvent.params.reason}):`);
    const callFrames = pausedEvent.params.callFrames;
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const name = frame.functionName || '(anonymous)';
      const url = frame.url || scripts.get(frame.location.scriptId) || '';
      const line = frame.location.lineNumber + 1;
      const column = frame.location.columnNumber + 1;
      testRunner.log(`    ${i}) ${name} (${url}:${line}:${column})`);
    }
    if (pausedEvent.params.asyncStackTrace) {
      testRunner.log(`    [${pausedEvent.params.asyncStackTrace.description}]`);
    }
  }

  async function runTestFunctionAndStep(actions) {
    testRunner.log('Set timer for test function.');
    session.evaluate('setTimeout(testFunction, 0);');
    let paused = await dp.Debugger.oncePaused();

    for (const action of actions) {
      if (action === 'Print') {
        printCallFrames(paused);
      } else if (action === 'StepInto') {
        testRunner.log('Executing StepInto...');
        const [stepResult, nextPaused] = await Promise.all([
          dp.Debugger.stepInto({breakOnAsyncCall: false}),
          dp.Debugger.oncePaused(),
        ]);
        if (stepResult.error) {
          testRunner.log(`FAIL: stepInto error: ${stepResult.error.message}`);
        }
        paused = nextPaused;
      } else if (action === 'StepOut') {
        testRunner.log('Executing StepOut...');
        const [stepResult, nextPaused] = await Promise.all([
          dp.Debugger.stepOut(),
          dp.Debugger.oncePaused(),
        ]);
        if (stepResult.error) {
          testRunner.log(`FAIL: stepOut error: ${stepResult.error.message}`);
        }
        paused = nextPaused;
      } else {
        testRunner.log(`FAIL: Unknown action ${action}`);
      }
    }

    const onUnexpectedPause = unexpected => {
      const top = unexpected.params.callFrames[0];
      const url = top.url || scripts.get(top.location.scriptId) || '';
      testRunner.log(`FAIL: unexpected pause in ${top.functionName} at ${url}:${
          top.location.lineNumber + 1}:${top.location.columnNumber + 1}`);
      dp.Debugger.resume({terminateOnResume: false});
    };
    dp.Debugger.onPaused(onUnexpectedPause);
    await dp.Debugger.resume({terminateOnResume: false});
    const textContent = await session.evaluateAsync(
        'new Promise(resolve => setTimeout(() => resolve(document.getElementById("myDiv").textContent), 0))');
    dp.Debugger.offPaused(onUnexpectedPause);
    testRunner.log('myDiv.textContent:');
    testRunner.log(textContent.trimEnd());
  }

  await runTestFunctionAndStep([
    'Print',
    'StepInto',
    'StepInto',
    'StepInto',
    'Print',
    'StepOut',
    'Print',
    'StepOut',
    'Print',
  ]);

  await runTestFunctionAndStep([
    'Print',
    'StepOut',
    'Print',
  ]);

  testRunner.completeTest();
});
