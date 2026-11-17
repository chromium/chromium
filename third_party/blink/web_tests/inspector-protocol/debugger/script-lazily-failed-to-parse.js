// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that FailedToParseScriptSource event is NOT raised after running a damaged part of a script that was already parsed.\n`);

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await dp.Runtime.evaluate({
    expression: `
      function thisIsBroken() {
        const duplicated = 1;
        const duplicated = 2;
      }
  `,
    objectGroup: 'console',
  });

  dp.Debugger.onScriptFailedToParse(() => {
    testRunner.log('ERROR: Recieved script failed to parse event');
  });
  await dp.Runtime.evaluate({
    expression: 'thisIsBroken()',
    objectGroup: 'console',
  });
  testRunner.log('Finished running broken code.');
  testRunner.completeTest();
});
