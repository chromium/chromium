// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp, session} = await testRunner.startURL(
      '../resources/blank.html',
      'Tests that Command Line API inspect() works correctly when paused');

  await Promise.all([
    dp.Runtime.enable(),
    dp.Debugger.enable(),
  ]);

  const pausedPromise = dp.Debugger.oncePaused();
  const evalPromise = session.evaluate('debugger;');
  const {params: {callFrames: [{callFrameId}]}} = await pausedPromise;

  const [{params: {object}}] = await Promise.all([
    dp.Runtime.onceInspectRequested(),
    dp.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: 'inspect($(\'body\'))',
      includeCommandLineAPI: true,
    }),
  ]);

  await Promise.all([
    dp.Debugger.resume(),
    evalPromise,
  ]);

  testRunner.log(object);
  testRunner.completeTest();
});
