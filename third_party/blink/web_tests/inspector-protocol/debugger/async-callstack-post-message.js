// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for window.postMessage.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.fail(`Unexpected exception: ${params.exceptionDetails.text}`);
  });
  await dp.Debugger.enable();

  const frameHTML = [
    '<!DOCTYPE html>',
    '<html>',
    '<body>',
    '<script>',
    'function onMessageReceivedInFrame(e) {',
    '    var msg = \'Received message: data="\' + e.data + \'" origin="\' + e.origin + \'"\';',
    '    if (e.data !== "skip debugger") debugger;',
    '    postMessageToParent(msg);',
    '}',
    '',
    'function postMessageToParent(msg)',
    '{',
    '    top.postMessage(msg, "*");',
    '}',
    '',
    'window.addEventListener("message", onMessageReceivedInFrame, false);',
    '//# sourceURL=post-message-listener.html',
    '</script>',
    '</body>',
    '</html>',
  ].join('\n');

  await session.evaluateAsync(frameHTML => {
    return new Promise((resolve, reject) => {
      const div = document.createElement('div');
      const iframe = document.createElement('iframe');
      iframe.id = 'iframe';
      iframe.width = '800';
      iframe.height = '100';
      iframe.style.border = '1px solid black';
      iframe.onload = resolve;
      iframe.onerror = () => reject(new Error('Failed to load iframe'));
      iframe.srcdoc = frameHTML;
      div.appendChild(iframe);
      document.body.appendChild(div);
    });
  }, frameHTML);

  await session.evaluate(`
      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          window.addEventListener("message", onMessageReceivedInParent, false);
          postMessageToSelf();
          postMessageToFrame("start");
      }

      function onMessageReceivedInParent(e)
      {
          debugger;
          if (/data="start"/.test(e.data || ""))
              postMessageToFrame("done");
      }

      function postMessageToSelf()
      {
          window.postMessage("message to myself", "*");
      }

      function postMessageToFrame(msg)
      {
          var iframe = document.getElementById("iframe");
          var win = iframe.contentWindow;
          win.postMessage(msg, "*");
      }
      //# sourceURL=async-callstack-post-message.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];
    function appendFrames(frames) {
      frames.forEach((frame, i) => {
        const {scriptId, lineNumber} = frame.location || frame;
        const url = (scripts.get(scriptId) || frame.url || '').split('/').pop();
        lines.push(`    ${i}) ${frame.functionName || '(anonymous)'} (${url}:${
            lineNumber + 1})`);
      });
    }
    appendFrames(callFrames);
    for (let st = asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      appendFrames(st.callFrames);
    }
    return lines.join('\n') + '\n';
  }

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  const totalDebuggerStatements = 5;
  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
    setTimeout(testFunction, 0);
    //# sourceURL=schedule-test-function.js
  `);

  const callStacksOutput = [];
  for (let step = 0; step < totalDebuggerStatements; ++step) {
    const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
        await pausedPromise;
    if (asyncStackTraceId) {
      testRunner.log('FAIL: Unexpected asyncStackTraceId');
    }
    callStacksOutput.push(formatStackTrace(callFrames, asyncStackTrace));
    if (step + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    } else {
      dp.Debugger.onPaused(() => {
        testRunner.log('FAIL: Unexpected extra pause');
        dp.Debugger.resume();
      });
    }
    await dp.Debugger.resume();
  }
  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
});
