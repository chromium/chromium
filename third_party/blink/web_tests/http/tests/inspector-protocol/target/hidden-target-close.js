// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(`Test closing hidden target`);

  function testRunnerLog() {
    testRunner.log(
        arguments[0] ?? '', arguments[1] ?? '', testRunner.stabilizeNames,
        ['sessionId', 'targetId']);
  }

  /**
   * Filter out the targets that are already present in the existing targets set
   * keeping only the new targets. If the result is an error, return the error.
   */
  function filterTargets(result) {
    if (result.error) {
      return result.error;
    }
    return result.result.targetInfos
        .filter(item => !existingTargets.has(item.targetId))
        .map(item => ({targetId: item.targetId, url: item.url}));
  }

  // Fill in the existing targets set with the current targets.
  const {result: targets} = await testRunner.browserP().Target.getTargets();
  const existingTargets =
      new Set(targets.targetInfos.map(target => target.targetId));

  testRunner.browserP().Target.onDetachedFromTarget((event) => {
    testRunnerLog(event, 'DetachedFromTarget');
  });

  // Auto attach is required to get notified when the hidden target is
  // destroyed.
  await testRunner.browserP().Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});


  testRunnerLog('Create hidden target');
  const {result: hiddenTarget} =
      await testRunner.browserP().Target.createTarget({
        url: 'about:blank?HIDDEN=TARGET',
        hidden: true,
      });

  // Verify the hidden target is visible from the browser session.
  testRunnerLog(
      filterTargets(await testRunner.browserP().Target.getTargets()),
      `Filtered targets via browser session: `);

  testRunnerLog('Close hidden target');
  testRunner.browserP().Target.closeTarget({targetId: hiddenTarget.targetId});

  // Wait for the hidden target to be detached.
  await testRunner.browserP().Target.onceDetachedFromTarget();

  // Verify the hidden target is not present anymore.
  testRunnerLog(
      filterTargets(await testRunner.browserP().Target.getTargets()),
      `Filtered targets via browser session: `);

  testRunner.completeTest();
})
