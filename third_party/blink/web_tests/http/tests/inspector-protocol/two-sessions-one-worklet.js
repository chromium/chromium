// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests attaching two sessions to one worklet.');

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  const modulePromise = session.evaluateAsync(`(function() {
      const url = '/inspector-protocol/target/resources/empty-worklet.js';
      const audioContext = new AudioContext();
      return audioContext.audioWorklet.addModule(url);
  })()`);

  const worklet = (await dp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'worklet')).params;
  testRunner.log(worklet, "Attached to worklet");
  const ws = session.createChild(worklet.sessionId);
  ws.protocol.Runtime.runIfWaitingForDebugger();

  const {targetId} = worklet.targetInfo;
  const attached2 = (await dp.Target.attachToTarget({targetId, flatten: true})).result;
  const ws2 = session.createChild(attached2.sessionId);
  await ws2.protocol.Runtime.runIfWaitingForDebugger();

  testRunner.log(attached2, "Attached to worklet once again");
  testRunner.completeTest();
});
