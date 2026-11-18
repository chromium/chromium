// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `<!DOCTYPE html>
      <input type="button" id="test">`,
      'Test that debugger will pause in all event listeners when corresponding breakpoint is set. Bug 77331.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.DOM.enable();
  await dp.Debugger.enable();
  await dp.DOMDebugger.enable();

  await dp.Runtime.evaluate({
    expression: `
      function listener1()
      {
      }

      function listener2()
      {
      }

      function listener3()
      {
      }

      function addListenerAndClick()
      {
          var element = document.getElementById("test");
          element.addEventListener("click", listener1, true);
          element.addEventListener("click", listener2, true);
          document.body.addEventListener("click", listener3, true);
          document.body.addEventListener("click", listener3, false);
          element.click();
      }
      //# sourceURL=step-through-event-listeners.js`,
    objectGroup: 'console',
  });

  function logCallFrames(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const url = scripts.get(frame.location.scriptId) || frame.url;
      testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    }
  }

  testRunner.log('\nRunning: testClickBreakpoint');
  await dp.DOMDebugger.setEventListenerBreakpoint({
    eventName: 'click',
    targetName: '*',
  });

  const evaluatePromise = dp.Runtime.evaluate({
    expression: 'addListenerAndClick()',
    objectGroup: 'console',
  });

  for (let i = 1; i <= 4; ++i) {
    const {params} = await dp.Debugger.oncePaused();
    testRunner.log('Script execution paused.');
    logCallFrames(params.callFrames);
    if (i === 4) {
      await dp.DOMDebugger.removeEventListenerBreakpoint({
        eventName: 'click',
        targetName: '*',
      });
    }
    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.resume({terminateOnResume: false});
    await resumedPromise;
    testRunner.log('Script execution resumed.');
  }

  await evaluatePromise;
  testRunner.completeTest();
});
