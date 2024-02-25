(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = 'http://127.0.0.1:8000/inspector-protocol/resources/inspector-protocol-page.html';
  const {session, dp} = await testRunner.startURL(pageURL,
    'Tests basic functionality of tab target.');

  const bp = testRunner.browserP();
  const {targetInfos} = (await bp.Target.getTargets({filter: [{type: "tab"}]})).result;
  const tabTargets = targetInfos.sort((a, b) => a.url.localeCompare(b.url));
  testRunner.log(tabTargets);
  const targetUnderTest = tabTargets.find(target => target.url === pageURL);

  const tabSessionId = (await bp.Target.attachToTarget({targetId: targetUnderTest.targetId, flatten: true})).result.sessionId;
  const tabSession = testRunner.browserSession().createChild(tabSessionId);
  const tp = tabSession.protocol;
  const tabTargetInfo = (await tp.Target.getTargetInfo());
  testRunner.log(tabTargetInfo, "Tab target info, as obtained from target");

  const autoAttachCompletePromise = tp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const autoAttachedTargets = [];
  tp.Target.onAttachedToTarget(target => {
    autoAttachedTargets.push(target.params);
  });
  // All auto-attaches should be complete before we return.
  await autoAttachCompletePromise;
  testRunner.log(autoAttachedTargets, "Auto-attached targets (there should be exactly 1): ");
  if (autoAttachedTargets.length !== 1) {
    testRunner.completeTest();
    return;
  }
  const frameSession = tabSession.createChild(autoAttachedTargets[0].sessionId);
  testRunner.log(`Attached to page ${await frameSession.evaluate('location.href')}`);
  // Now create a cross-process subframe and make sure it only gets attached to the
  // frame target, not to the tab one.
  await frameSession.protocol.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const subframePromise = frameSession.protocol.Target.onceAttachedToTarget();
  await frameSession.evaluateAsync(`new Promise(resolve => {
    const frame = document.createElement('iframe');
    frame.src = 'http://devtools.oopif-a.test:8000/inspector-protocol/resources/inspector-protocol-page.html';
    frame.onload = resolve;
    document.body.appendChild(frame);
  })`);
  testRunner.log(await subframePromise, `Auto-attached subframe target`);
  testRunner.log(`Number of auto-attached tab sessions (should be 1): ${autoAttachedTargets.length}`);
  testRunner.completeTest();
});
