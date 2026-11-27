// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that uncaught promise rejections happened in workers are logged with stack traces.');

  const debuggers = new Map();
  await dp.Runtime.enable();
  const pageDebuggerId = (await dp.Debugger.enable()).result.debuggerId;
  debuggers.set(pageDebuggerId, dp.Debugger);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const workerUrl =
      testRunner.url('../resources/worker-with-unhandled-promises.js');
  await session.evaluate(`
    var worker;

    function startWorker() {
      worker = new Worker('${workerUrl}');
      worker.postMessage('');
    }
    //# sourceURL=console-uncaught-promise-in-worker.js
  `);

  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    setTimeout(startWorker, 0);
    //# sourceURL=console-uncaught-promise-in-worker.js
  `);

  const childSession =
      session.createChild((await attachedPromise).params.sessionId);
  const exceptions = [];
  let resolveTwoExceptions;
  const twoExceptionsPromise = new Promise(resolve => {
    resolveTwoExceptions = resolve;
  });
  childSession.protocol.Runtime.onExceptionThrown(event => {
    exceptions.push(event.params.exceptionDetails);
    if (exceptions.length === 2) {
      resolveTwoExceptions();
    }
  });

  const workerDebuggerId =
      (await childSession.protocol.Debugger.enable()).result.debuggerId;
  debuggers.set(workerDebuggerId, childSession.protocol.Debugger);
  await childSession.protocol.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  await childSession.protocol.Runtime.enable();
  await childSession.protocol.Runtime.runIfWaitingForDebugger();

  await twoExceptionsPromise;

  async function logNormalizedStackTrace(stackTrace, debuggerId) {
    while (stackTrace) {
      const {description, callFrames, parent, parentId} = stackTrace;
      if (description) {
        testRunner.log(`--${description}--`);
      }
      for (const frame of callFrames) {
        const functionName = frame.functionName || '(anonymous)';
        const url = testRunner.trimURL(frame.url);
        testRunner.log(`${functionName} at ${url}:${frame.lineNumber}:${
            frame.columnNumber}`);
      }
      if (parentId) {
        if (parentId.debuggerId) {
          debuggerId = parentId.debuggerId;
        }
        const result = await debuggers.get(debuggerId).getStackTrace({
          stackTraceId: parentId,
        });
        stackTrace = result.stackTrace || result.result.stackTrace;
      } else {
        stackTrace = parent;
      }
    }
  }

  for (const details of exceptions) {
    const errorLine = details.exception?.description?.split('\n')[0] || '';
    testRunner.log(`${details.text} ${errorLine}`.trim());
    await logNormalizedStackTrace(details.stackTrace, workerDebuggerId);
  }

  const detachedPromise = dp.Target.onceDetachedFromTarget();
  await session.evaluate('worker.terminate();');
  await detachedPromise;

  testRunner.completeTest();
});
