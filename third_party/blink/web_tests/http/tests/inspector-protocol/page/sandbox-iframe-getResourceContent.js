(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that DevTools can request the content of a sandboxed iframe without a crash (crbug.com/500193041)');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluate(`
    const iframe = document.createElement('iframe');
    iframe.id = 'f';
    iframe.sandbox = 'allow-scripts';
    iframe.src = 'http://devtools.oopif-a.test:8000/inspector-protocol/network/resources/cached.php';
    document.body.appendChild(iframe);
  `);

  const {sessionId, targetInfo} =
      (await dp.Target.onceAttachedToTarget()).params;
  const iframeSession = session.createChild(sessionId);
  const navigated = iframeSession.protocol.Page.onceFrameNavigated();
  await iframeSession.protocol.Page.enable();
  await iframeSession.protocol.Network.enable();
  await iframeSession.protocol.Runtime.runIfWaitingForDebugger();
  const navigatedEvent = await navigated;

  testRunner.log(`frame.url: ${navigatedEvent.params.frame.url}`);
  testRunner.log(await iframeSession.protocol.Page.getResourceContent(
      {frameId: targetInfo.targetId, url: navigatedEvent.params.frame.url}));
  testRunner.completeTest();
});
