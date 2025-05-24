// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(`Test page enable in hidden target`);

  function testRunnerLog() {
    testRunner.log(
        arguments[0] ?? '', arguments[1] ?? '', testRunner.stabilizeNames,
        ['sessionId', 'targetId']);
  }


  testRunnerLog('Create hidden target');
  const {result: hiddenTarget} =
      await testRunner.browserP().Target.createTarget({
        url: 'about:blank?HIDDEN=TARGET',
        hidden: true,
      });

  testRunner.browserP().Target.attachToTarget(
      {targetId: hiddenTarget.targetId, flatten: true});
  const attachedToHiddenTargetEvent =
      await testRunner.browserP().Target.onceAttachedToTarget();
  testRunnerLog('Attached to the hidden target');

  const hiddenSession = new TestRunner.Session(
      testRunner, attachedToHiddenTargetEvent.params.sessionId);

  testRunnerLog('Enable page');
  testRunnerLog(await hiddenSession.protocol.Page.enable());

  testRunner.completeTest();
})
