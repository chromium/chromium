// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Debugger.setBreakpointByUrl with urlRegex.');

  await dp.Debugger.enable();
  await session.evaluate(`${'\n'.repeat(10)}
      function testFunction()
      {
          foo();
      }

      function foo()
      {
      }
      //# sourceURL=debugger-set-breakpoint-regex.js
  `);

  testRunner.runTestSuite([
    async function testSetNoneOfURLAndRegex() {
      const response = await dp.Debugger.setBreakpointByUrl({lineNumber: 1});
      testRunner.log(response.error.message);
    },

    async function testSetBothURLAndRegex() {
      const url = 'debugger-set-breakpoint.js';
      const urlRegex = 'debugger-set-breakpoint.*';
      const response =
          await dp.Debugger.setBreakpointByUrl({lineNumber: 1, url, urlRegex});
      testRunner.log(response.error.message);
    },

    async function testSetByRegex() {
      await dp.Debugger.setBreakpointByUrl(
          {urlRegex: 'debugger-set-breakpoint.*', lineNumber: 11});
      const pausedPromise = dp.Debugger.oncePaused();
      testRunner.log('Set timer for test function.');
      await session.evaluate('setTimeout(testFunction, 0)');
      const {params: {callFrames, hitBreakpoints}} = await pausedPromise;
      testRunner.log('Script execution paused.');
      testRunner.log(hitBreakpoints, 'Hit breakpoints: ');
      testRunner.log('Call stack:');
      callFrames.forEach((f, i) => {
        testRunner.log(
            `    ${i}) ${f.functionName} (debugger-set-breakpoint-regex.js:${
                f.location.lineNumber + 1})`);
      });
      await dp.Debugger.resume();
      testRunner.log('Script execution resumed.');
    }
  ]);
});
