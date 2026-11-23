// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div id="myDiv"></div>`,
      'Tests that stepping into dispatchEvent() method will lead to a pause in the first event listener.\n');

  await dp.Debugger.enable();

  await session.evaluate(`
    function testFunction() {
      var div = document.getElementById('myDiv');
      function fooEventHandler1() {
        div.textContent += 'Recieved foo event(1)!\\n';
      }
      div.addEventListener('foo', fooEventHandler1);

      function fooEventHandler2() {
        div.textContent += 'Recieved foo event(2)!\\n';
      }
      div.addEventListener('foo', fooEventHandler2);

      var e = new CustomEvent('foo');
      debugger;
      div.dispatchEvent(e);
    }
    //# sourceURL=step-into-event-listener.js
  `);

  function checkTopFrameFunction(callFrames, expectedName, reason) {
    const topFunctionName = callFrames[0].functionName;
    if (expectedName === topFunctionName) {
      testRunner.log('SUCCESS: Did step into event listener(' + expectedName +
                     ').');
    } else {
      testRunner.log('FAIL: Unexpected top function: expected ' + expectedName +
                     ', found ' + topFunctionName);
    }
    if (reason !== 'step') {
      testRunner.log('FAIL: wrong pause reason: ' + reason);
    }
  }

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('setTimeout(testFunction, 0)');

  for (let stepCount = 0; stepCount <= 5; ++stepCount) {
    const {params: {callFrames, reason}} = await pausedPromise;
    if (stepCount === 2) {
      checkTopFrameFunction(callFrames, 'fooEventHandler1', reason);
    } else if (stepCount === 5) {
      checkTopFrameFunction(callFrames, 'fooEventHandler2', reason);
    }

    if (stepCount < 5) {
      testRunner.log('Stepping into...');
      pausedPromise = dp.Debugger.oncePaused();
      const resumedPromise = dp.Debugger.onceResumed();
      await dp.Debugger.stepInto({breakOnAsyncCall: false});
      await resumedPromise;
    }
  }

  let extraPause = false;
  dp.Debugger.onPaused(async () => {
    extraPause = true;
    await dp.Debugger.resume();
  });
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;

  const finalText =
      await session.evaluate(`document.getElementById('myDiv').textContent`);
  if (extraPause) {
    testRunner.log('FAIL: unexpected extra pause after completion.');
  }
  if (finalText !== 'Recieved foo event(1)!\nRecieved foo event(2)!\n') {
    testRunner.log(
        `FAIL: unexpected div.textContent: ${JSON.stringify(finalText)}`);
  }

  testRunner.completeTest();
})
