// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests exception message from eval on worker context contains stack trace.');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  const workerReadyPromise = session.evaluateAsync(`
    new Promise(resolve => {
      window.worker = new Worker('${
      testRunner.url('./resources/worker-eval.js')}');
      window.worker.onmessage = e => resolve(e.data);
    })
  `);

  const {params: {sessionId, targetInfo}} = await attachedPromise;
  const workerReady = await workerReadyPromise;
  testRunner.log({
    targetType: targetInfo.type,
    targetScript: targetInfo.url.split('/').pop(),
    workerReady,
  });

  const childSession = session.createChild(sessionId);
  const contextCreatedPromise =
      childSession.protocol.Runtime.onceExecutionContextCreated();
  await childSession.protocol.Runtime.enable();
  await childSession.protocol.Debugger.enable();
  const {params: {context}} = await contextCreatedPromise;

  const expression = [
    '            function foo()',
    '            {',
    '                throw {a:239};',
    '            }',
    '            function boo()',
    '            {',
    '                foo();',
    '            }',
    '            boo();',
  ].join('\n');

  testRunner.log(expression);
  const {result: {exceptionDetails}} =
      await childSession.protocol.Runtime.evaluate({
        expression,
        contextId: context.id,
        objectGroup: 'console',
        includeCommandLineAPI: true,
        silent: false,
        returnByValue: false,
        generatePreview: true,
        replMode: true,
      });

  if (!exceptionDetails) {
    testRunner.fail('Expected exceptionDetails from Runtime.evaluate');
    return;
  }

  const propsResponse = await childSession.protocol.Runtime.getProperties({
    objectId: exceptionDetails.exception.objectId,
    ownProperties: true,
  });

  testRunner.log({
    text: exceptionDetails.text,
    line: exceptionDetails.lineNumber + 1,
    exception: {
      type: exceptionDetails.exception.type,
      className: exceptionDetails.exception.className,
      description: exceptionDetails.exception.description,
      preview: exceptionDetails.exception.preview,
      ownProperties: propsResponse.result.result.map(prop => ({
                                                       name: prop.name,
                                                       type: prop.value.type,
                                                       value: prop.value.value,
                                                     })),
    },
    callFrames: exceptionDetails.stackTrace.callFrames.map(
        frame => ({
          functionName: frame.functionName || '(anonymous)',
          line: frame.lineNumber + 1,
          column: frame.columnNumber + 1,
        })),
  });

  testRunner.completeTest();
});
