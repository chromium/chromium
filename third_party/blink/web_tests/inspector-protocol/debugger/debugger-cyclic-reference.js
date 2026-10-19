// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that pausing and inspecting scopes when Object.prototype has a cyclic reference does not crash (crbug.com/43558).`);

  await dp.Debugger.enable();
  await dp.Runtime.enable();

  await dp.Runtime.evaluate({
    expression: `
      Object.prototype.cyclicRef = Object.prototype;

      function testFunction()
      {
          var o = new Object();
          debugger;
      }
    `,
  });

  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0)');
  testRunner.log('Set timer for test function.');

  const {params} = await pausedPromise;
  testRunner.log('Script execution paused.');

  await dp.Runtime.getProperties({
    objectId: params.callFrames[0].scopeChain[0].object.objectId,
  });

  await dp.Debugger.resume();
  testRunner.log('Script execution resumed.');
  testRunner.completeTest();
});
