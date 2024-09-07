// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async () => {
  async function initTest(testRunner, dp, session, navigationHelper) {
    const stableNames = ['sessionId', 'callFrames'];
    const stableValues = ['parentFrameId', 'frameId', 'targetId',];

    const logEvent = (event) => {
      testRunner.log(event, 'Received event', stableNames, stableValues);
    };

    /**
     * Set up the CDP protocol target. Sets events listeners, sets auto attach and
     * enables the Page domain.
     */
    await navigationHelper.initProtocolRecursively(dp, session,
        async (dp) => {
          dp.Page.onFrameDetached(event => {
            if (event.params.reason !== 'swap') {
              logEvent(event);
            }
          });
          dp.Page.onFrameSubtreeWillBeDetached(logEvent);
        });
  }

  async function createAndWaitIframe(testRunner, session, navigationHelper, url) {
    const nestedIframeLoadedPromise = navigationHelper.onceFrameStoppedLoading(
        'resources/empty.html');
    testRunner.log(`... creating and navigating iframe ${url}`);
    await session.evaluate(`
      window.frame = document.createElement('iframe');
      frame.src = '${url}';
      document.body.appendChild(frame);
    `);
    await nestedIframeLoadedPromise;
    testRunner.log(`... iframe created and navigated`);
  }

  return {createAndWaitIframe, initTest};
})();