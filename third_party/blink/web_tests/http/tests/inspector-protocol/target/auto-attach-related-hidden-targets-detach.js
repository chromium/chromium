// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that navigating a page with an auto-attached iframe and hidden targets detaches cleanly without crashing.');
  const bp = testRunner.browserP();

  const page = await testRunner.createPage();
  const pageSession = await page.createSession();

  // Auto-attach related targets for the page.
  await bp.Target.autoAttachRelated({
    targetId: page.targetId(),
    waitForDebuggerOnStart: false,
  });

  // Create an OOPIF.
  const [attached] = await Promise.all([
    bp.Target.onceAttachedToTarget(),
    pageSession.evaluate(() => {
      const iframe = document.createElement('iframe');
      iframe.src = 'http://devtools.oopif.test:8080/inspector-protocol/resources/iframe.html';
      document.body.appendChild(iframe);
    }),
  ]);
  const iframeSession = testRunner.createSessionFor(attached.params.sessionId);

  // Create hidden targets under the iframe session and auto-attach them.
  const kHiddenTargetCount = 20;
  for (let i = 0; i < kHiddenTargetCount; ++i) {
    const {result: hiddenTarget} =
        await iframeSession.protocol.Target.createTarget({
          url: 'about:blank',
          hidden: true,
        });
    await bp.Target.autoAttachRelated({
      targetId: hiddenTarget.targetId,
      waitForDebuggerOnStart: false,
    });
  }

  // Cross-site navigation detaches the iframe and disposes its hidden targets.
  await page.navigate('http://devtools.oopif-b.test:8080/inspector-protocol/resources/iframe.html');

  // Assert that iframeSession was cleanly detached.
  const evalResult = await iframeSession.protocol.Runtime.evaluate({expression: '1 + 1'});
  if (evalResult.error) {
    testRunner.log('iframe session detached');
  } else {
    testRunner.log('ERROR: iframe session should be detached but it was not');
  }

  testRunner.log('SUCCESS: Navigated and detached without crash');
  testRunner.completeTest();
});
