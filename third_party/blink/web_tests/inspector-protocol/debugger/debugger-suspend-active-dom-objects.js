// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Runtime.evaluate calling alert() while paused on a breakpoint with suspended active DOM objects does not crash.`);

  await dp.Debugger.enable();
  await dp.Runtime.enable();

  await session.evaluate(`
      function testFunction() {
          setTimeout("void 0", 0); // Create a timer that will be suspended on a breakpoint.
          debugger;
      }
  `);

  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0)');
  testRunner.log('Set timer for test function.');

  await pausedPromise;
  testRunner.log('Script execution paused.');

  await session.evaluate('alert(1)');
  testRunner.log('Shown alert while staying on a breakpoint.');

  await dp.Debugger.resume();
  testRunner.log('Script execution resumed.');
  testRunner.completeTest();
});
