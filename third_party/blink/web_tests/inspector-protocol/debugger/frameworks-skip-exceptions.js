// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<script src="${testRunner.url('resources/framework.js')}"></script>`,
      'Tests that Debugger.setBlackboxPatterns skips exceptions thrown in blackboxed scripts, including those inside V8 native scripts.');

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Debugger.enable();
  await session.evaluate(`${'\n'.repeat(19)}
      function testFunction()
      {
          Framework.throwFrameworkExceptionAndCatch();
          Framework.throwInNativeAndCatch();

          // All above should be skipped.
          debugger;
          try { throw new Error('NotBlackboxed'); } catch (e) {}
      }
      //# sourceURL=frameworks-skip-exceptions.js
  `);

  await dp.Debugger.setBlackboxPatterns({patterns: ['/framework\\.js$']});
  await dp.Debugger.setPauseOnExceptions({state: 'all'});

  function logPause({reason, callFrames}) {
    testRunner.log(`Paused with reason: ${reason}`);
    testRunner.log('Call stack:');
    callFrames.forEach((f, i) => {
      const fileName =
          (scripts.get(f.location.scriptId) || '').split('/').pop();
      testRunner.log(`    ${i}) ${f.functionName} (${fileName}:${
          f.location.lineNumber + 1})`);
    });
  }

  testRunner.log('Set timer for test function.');
  let pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0)');
  logPause((await pausedPromise).params);

  // An exception thrown outside of blackboxed scripts must still pause.
  pausedPromise = dp.Debugger.oncePaused();
  await dp.Debugger.resume();
  logPause((await pausedPromise).params);

  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  await dp.Debugger.resume();
  testRunner.completeTest();
});
