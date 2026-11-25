// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that callFrames on pause contains function name taking into ' +
      'account Function.name (and ignoring displayName).\n');

  await dp.Debugger.enable();
  await session.evaluate(`
      var foo = function ()
      {
          debugger;
      }

      foo.displayName = 'foo.displayName';
      Object.defineProperty(foo, 'name', { value: 'foo.function.name' } );

      var bar = function()
      {
          foo();
      }

      bar.displayName = 'bar.displayName';

      var baz = function()
      {
          bar();
      }

      Object.defineProperty(baz, 'name', { value: 'baz.function.name' } );

      function testFunction()
      {
          baz();
      }
  `);

  testRunner.log('Set timer for test function.');
  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0);');
  const {params: {callFrames}} = await pausedPromise;

  testRunner.log('Script execution paused.');
  testRunner.log('callFrames.length = ' + callFrames.length);
  for (let i = 0; i < callFrames.length; ++i) {
    testRunner.log('functionName: ' + callFrames[i].functionName);
  }

  let extraPause = false;
  dp.Debugger.onPaused(() => {
    extraPause = true;
  });
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  await session.evaluate('0');
  if (extraPause) {
    testRunner.fail('Unexpected extra pause.');
    return;
  }

  testRunner.log('Script execution resumed.');
  testRunner.completeTest();
});
