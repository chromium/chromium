// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests console output from PaintWorklet.');

  function formatMessage(event) {
    const {type, args, stackTrace} = event.params;
    const frame = stackTrace.callFrames[0];
    return {
      type,
      text: args.map(arg => arg.value).join(' '),
      url: frame.url,
      lineNumber: frame.lineNumber,
      columnNumber: frame.columnNumber,
      functionName: frame.functionName,
    };
  }

  function compareMessages(a, b) {
    return a.lineNumber - b.lineNumber || a.type.localeCompare(b.type) ||
        a.text.localeCompare(b.text);
  }

  await dp.Runtime.enable();
  const mainMessagesByContext = new Map();
  let resolveMainDone;
  const mainDone = new Promise(resolve => {
    resolveMainDone = resolve;
  });
  dp.Runtime.onConsoleAPICalled(event => {
    const {executionContextId} = event.params;
    let messages = mainMessagesByContext.get(executionContextId);
    if (!messages) {
      messages = [];
      mainMessagesByContext.set(executionContextId, messages);
    }
    messages.push(formatMessage(event));
    if (mainMessagesByContext.size === 2 &&
        Array.from(mainMessagesByContext.values())
            .every(msgs => msgs.length === 3)) {
      resolveMainDone();
    }
  });

  const workletMessages = [];
  let resolveWorkletsDone;
  const workletsDone = new Promise(resolve => {
    resolveWorkletsDone = resolve;
  });

  dp.Target.onAttachedToTarget(async event => {
    if (event.params.targetInfo.type !== 'worklet') {
      return;
    }
    const workletSession = session.createChild(event.params.sessionId);
    const messages = [];
    workletMessages.push(messages);
    workletSession.protocol.Runtime.onConsoleAPICalled(msg => {
      messages.push(formatMessage(msg));
      if (workletMessages.length === 2 &&
          workletMessages.every(msgs => msgs.length === 3)) {
        resolveWorkletsDone();
      }
    });
    await workletSession.protocol.Runtime.enable();
    await workletSession.protocol.Runtime.runIfWaitingForDebugger();
  });

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await session.evaluateAsync(`CSS.paintWorklet.addModule('${
      testRunner.url('resources/console-worklet-script.js')}')`);
  await Promise.all([mainDone, workletsDone]);

  const sortedMainMessages = Array.from(mainMessagesByContext.entries())
                                 .sort((a, b) => a[0] - b[0])
                                 .map(([, msgs]) => msgs.sort(compareMessages));
  for (const messages of workletMessages) {
    messages.sort(compareMessages);
  }
  workletMessages.sort((a, b) => compareMessages(a[0], b[0]));

  testRunner.log(sortedMainMessages,
                 'Main thread PaintWorklet console messages:');
  testRunner.log(workletMessages, 'Off-thread PaintWorklet console messages:');
  testRunner.completeTest();
});
