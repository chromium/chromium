// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that debugger breaks on failed assertion when pause on exception mode is enabled.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const scriptParsedPromise = dp.Debugger.onceScriptParsed(
      ({params}) => params.url.endsWith('pause-on-failed-assertion.js'));
  await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(13)}
      function failAssertion()
      {
          console.assert(false);
      }

      function handleClick()
      {
          failAssertion();
      }
      //# sourceURL=pause-on-failed-assertion.js
  `,
    objectGroup: 'console',
  });
  const {params: {scriptId}} = await scriptParsedPromise;

  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});
  await dp.Debugger.getScriptSource({scriptId});
  testRunner.log('Script source was shown.');

  const pausedPromise = dp.Debugger.oncePaused();
  const consolePromise =
      dp.Runtime.onceConsoleAPICalled(({params}) => params.type === 'assert');
  await session.evaluate('setTimeout(handleClick, 0)');
  const {params: {callFrames, reason}} = await pausedPromise;
  await consolePromise;
  if (reason !== 'assert') {
    testRunner.fail(`Expected pause reason "assert", got "${reason}".`);
    return;
  }
  testRunner.log('Script execution paused.');

  const lines = [
    'Call stack:',
    ...callFrames.map((frame, i) => {
      const {scriptId, lineNumber} = frame.location;
      const url = (scripts.get(scriptId) || '').split('/').pop();
      return `    ${i}) ${frame.functionName} (${url}:${lineNumber + 1})`;
    }),
  ];
  testRunner.log(lines.join('\n'));

  let extraPause = false;
  dp.Debugger.onPaused(() => {
    extraPause = true;
    void dp.Debugger.resume({terminateOnResume: false});
  });
  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  await dp.Debugger.setBreakpointsActive({active: true});
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');
  if (extraPause) {
    testRunner.fail('Unexpected extra pause.');
    return;
  }

  testRunner.completeTest();
});
