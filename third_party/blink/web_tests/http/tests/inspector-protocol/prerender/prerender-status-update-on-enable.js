// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  async function setupSession() {
    const { tabTargetSession } = await testRunner.startBlankWithTabTarget(
      `Test that a status update is sent for a completed prerender on enable`);

    const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
    await childTargetManager.startAutoAttach();
    const session = childTargetManager.findAttachedSessionPrimaryMainFrame();
    const dp = session.protocol;

    return {session, dp};
  }

  async function waitForPrerender(dp, expectedStatus) {
    await dp.Preload.oncePrerenderStatusUpdated(e => e.params.status === expectedStatus);
  }

  async function testReady() {
    const {session, dp} = await setupSession();
    await dp.Preload.enable();
    session.navigate('resources/simple-prerender.html');

    // Wait for prerender to complete.
    await waitForPrerender(dp, 'Ready');

    // Disable and re-enable Preload domain.
    await dp.Preload.disable();
    dp.Preload.enable();

    // Status update should be sent with 'Ready' status.
    const prerenderStatusUpdated = (await dp.Preload.oncePrerenderStatusUpdated()).params;
    testRunner.log(prerenderStatusUpdated);
  }

  async function testFailure() {
    const {session, dp} = await setupSession();
    await dp.Preload.enable();
    session.navigate('resources/bad-http-prerender.html');

    // Wait for prerender to fail.
    await waitForPrerender(dp, 'Failure');

    // Disable and re-enable Preload domain.
    await dp.Preload.disable();
    dp.Preload.enable();

    // Status update should be sent with 'Failure' status.
    const prerenderStatusUpdated = (await dp.Preload.oncePrerenderStatusUpdated()).params;
    testRunner.log(prerenderStatusUpdated);
  }

  async function testCandidateRemovedAfterFailure() {
    const {session, dp} = await setupSession();
    await dp.Preload.enable();
    session.navigate('resources/bad-http-prerender.html');
    await waitForPrerender(dp, 'Failure');

    // Remove speculation rule and disable Preload.
    session.evaluate(() => {
      document.querySelector('script').remove();
    });
    await dp.Preload.disable();

    // Re-enable Preload; we should not get any status update for the removed
    // speculation candidate.
    dp.Preload.onPrerenderStatusUpdated(() => {
      testRunner.fail('Status update received for removed speculation candidate.');
    });
    await dp.Preload.enable();
    testRunner.log('No status update received after Preload domain is re-enabled.');
  }

  testRunner.runTestSuite([
    testReady,
    testFailure,
    testCandidateRemovedAfterFailure
  ]);
});
