// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that text-size-adjust is updated when mobile emulation is toggled on a second page in the same renderer process.');

  const mobileMetrics = {
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
    mobile: true,
    scale: 1,
  };

  // Enable mobile emulation on the first page so the process-wide
  // ScopedGlobalOverrides is already held.
  await dp.Emulation.setDeviceMetricsOverride(mobileMetrics);

  // Open a second page via window.open so both pages share a renderer process.
  const target = testRunner.browserP().Target;
  await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const attachedPromise = target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'page');
  const openPromise =
      session.evaluate(`window.popup = window.open('about:blank'); undefined;`);
  const attachedEvent = await attachedPromise;
  const popupSession =
      testRunner.createSessionFor(attachedEvent.params.sessionId);
  const popupDp = popupSession.protocol;
  await popupDp.Runtime.runIfWaitingForDebugger();
  await openPromise;

  await popupSession.evaluate(() => {
    document.body.innerHTML =
        '<div id="target" style="font-size: 10px; text-size-adjust: 200%;">Test</div>';
  });

  async function logFontSize() {
    testRunner.log(await popupSession.evaluate(() => {
      const target = document.getElementById('target');
      return window.getComputedStyle(target).fontSize;
    }));
  }

  testRunner.log('Before emulation:');
  await logFontSize();

  testRunner.log('Enabling mobile emulation:');
  await popupDp.Emulation.setDeviceMetricsOverride(mobileMetrics);
  await logFontSize();

  testRunner.log('Clearing emulation:');
  await popupDp.Emulation.clearDeviceMetricsOverride();
  await logFontSize();

  await target.closeTarget({targetId: attachedEvent.params.targetInfo.targetId});
  testRunner.completeTest();
})
