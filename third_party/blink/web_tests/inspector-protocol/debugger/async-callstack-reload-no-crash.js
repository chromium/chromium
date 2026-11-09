// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Page.reload while paused with Debugger.setAsyncCallStackDepth enabled does not crash. Bug 441223.');

  await dp.Page.enable();
  await dp.Debugger.enable();
  await session.evaluate(`
      function testFunction()
      {
          setInterval(callback, 0);
          debugger;
      }

      function callback()
      {
          return window._foo;
      }
  `);

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 8});
  const pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('setTimeout(testFunction, 0)');
  await pausedPromise;

  testRunner.log('Reloading the page...');
  const loadPromise = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await loadPromise;
  testRunner.log('Page reloaded.');
  const {result} = await dp.Runtime.evaluate({expression: '1'});
  testRunner.log(`Evaluated after reload: ${result.result.value}`);
  testRunner.log('PASS: Reloaded successfully.');
  testRunner.completeTest();
});
