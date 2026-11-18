// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div id="myDiv"></div>`,
      'Tests that scheduled pause is cleared after processing event with empty handler.');

  await dp.DOM.enable();
  const debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  const debuggers = new Map([[debuggerId, dp.Debugger]]);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
    function SHOULD_NOT_STOP_HERE() {
      return 239;
    }

    function addEmptyEventListenerAndClick() {
      // this event listener won't execute any JS code.
      const div = document.getElementById('myDiv');
      div.addEventListener('click', {});
      div.click();
      SHOULD_NOT_STOP_HERE();
      debugger;
    }
  `);

  await dp.DOMDebugger.setEventListenerBreakpoint({
    eventName: 'click',
    targetName: '*',
  });

  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(addEmptyEventListenerAndClick, 1)');
  const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
      await pausedPromise;

  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);

  await dp.DOMDebugger.removeEventListenerBreakpoint({
    eventName: 'click',
    targetName: '*',
  });
  await dp.Debugger.resume();

  testRunner.completeTest();
});
