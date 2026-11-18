// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that pause on exception in internal script does not crash.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const scriptParsedPromise = dp.Debugger.onceScriptParsed();
  await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(14)}function throwException()
      {
          new Function("return ()");
      }

      function handleClick()
      {
          throwException();
      }
  //# sourceURL=test://evaluations/0/debugger-pause-in-internal.js`,
    objectGroup: 'console',
  });
  const {params: {scriptId}} = await scriptParsedPromise;

  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});
  await dp.Debugger.getScriptSource({scriptId});
  testRunner.log('Script source was shown.');

  const failedToParsePromise = dp.Debugger.onceScriptFailedToParse();
  const pausedPromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate({
    expression:
        `${
        '\n'.repeat(
            35)}setTimeout(handleClick, 0)//# sourceURL=test://evaluations/1/debugger-pause-in-internal.js`,
    objectGroup: 'console',
  });
  await failedToParsePromise;
  const {params: {callFrames}} = await pausedPromise;
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

  await dp.Debugger.setBreakpointsActive({active: true});
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  testRunner.completeTest();
});
