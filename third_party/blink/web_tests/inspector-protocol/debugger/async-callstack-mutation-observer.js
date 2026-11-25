// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for MutationObserver.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
    var node = document.createElement('div');
    var nestedNode = document.createElement('div');

    var observer = new MutationObserver(mutationCallback);
    var nestedObserver = new MutationObserver(nestedMutationCallback);

    var timeoutFromMutationCallback;
    var timeoutFromNestedMutationCallback;
    var lastDoMutationsFunc;

    function mutationCallback(mutations) {
      if (lastDoMutationsFunc == doMutations1)
        doMutations1(nestedNode);
      debugger;
      if (!timeoutFromMutationCallback)
        timeoutFromMutationCallback = setTimeout(timeoutFromMutation, 0);
    }

    function nestedMutationCallback(mutations) {
      debugger;
      if (!timeoutFromNestedMutationCallback) {
        timeoutFromNestedMutationCallback =
            setTimeout(timeoutFromNestedMutation, 0);
      }
    }

    function testFunction() {
      setTimeout(timeout1, 0);
      var config = {attributes: true, childList: true, characterData: true};
      observer.observe(node, config);
      nestedObserver.observe(nestedNode, config);
    }

    function timeout1() {
      setTimeout(timeout2, 0);
      doMutations1(node);
    }

    function doMutations1(node) {
      lastDoMutationsFunc = doMutations1;
      node.setAttribute('foo', 'bar');
      node.className = 'c1 c2';
      node.textContent = 'text';
    }

    function timeout2() {
      doMutations2(node);
    }

    function doMutations2(node) {
      lastDoMutationsFunc = doMutations2;
      var child = document.createElement('span');
      node.appendChild(child);
      node.textContent = '';
    }

    function timeoutFromMutation() {
      debugger;
    }

    function timeoutFromNestedMutation() {
      debugger;
      doMutations2(nestedNode);
    }
    //# sourceURL=async-callstack-mutation-observer.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace) {
    const results = ['Call stack:'];
    function printFrames(frames) {
      let printed = 0;
      for (const frame of frames) {
        const {scriptId, lineNumber} = frame.location || frame;
        const url = frame.url || scripts.get(scriptId) || '';
        const source = url ? url.split('/').pop() : '<anonymous>';
        results.push(`    ${printed++}) ${
            frame.functionName ||
            '(anonymous)'} (${source}:${lineNumber + 1})`);
      }
    }
    printFrames(callFrames);
    for (let st = asyncStackTrace; st; st = st.parent) {
      results.push(`    [${st.description || 'Async Call'}]`);
      printFrames(st.callFrames);
    }
    return results.join('\n');
  }

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
    setTimeout(testFunction, 0);
    //# sourceURL=schedule-test-function.js
  `);

  const totalDebuggerStatements = 6;
  const callStacksOutput = [];
  for (let i = 0; i < totalDebuggerStatements; ++i) {
    const {params: {callFrames, asyncStackTrace}} = await pausedPromise;
    callStacksOutput.push(formatStackTrace(callFrames, asyncStackTrace));
    if (i + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    }
    await dp.Debugger.resume();
  }

  // Drain any remaining scheduled timers and fail if an unexpected pause
  // occurs.
  let unexpectedPauseCount = 0;
  dp.Debugger.onPaused(async () => {
    unexpectedPauseCount++;
    await dp.Debugger.resume();
  });
  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');
  if (unexpectedPauseCount !== 0) {
    testRunner.log(`ERROR: ${unexpectedPauseCount} unexpected pauses`);
  }

  testRunner.log('Captured call stacks in no particular order:');
  testRunner.log(callStacksOutput.sort().join('\n'));
  testRunner.completeTest();
})
