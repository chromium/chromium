(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      `Assure page is not re-attached during WC being closed.`);

  const target = testRunner.browserP().Target;
  const response = await target.attachToBrowserTarget();

  const browserSession =
      new TestRunner.Session(testRunner, response.result.sessionId);
  const newUrl = 'http://cross-site.test:8080/inspector-protocol/resources/test-page.html';
  const {result} = await browserSession.protocol.Target.createTarget({
                     url: newUrl, forTab: true });
  const attachedToTab1 = (await
      target.attachToTarget({targetId: result.targetId, flatten: true})).result;
  const dp1 = browserSession.createChild(attachedToTab1.sessionId).protocol;

  dp1.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp1.Target.setDiscoverTargets({discover: true});

  const attachedToPage = (await dp1.Target.onceAttachedToTarget()).params;
  const pageSession = browserSession.createChild(attachedToPage.sessionId);
  pageSession.protocol.Page.enable();
  pageSession.protocol.Page.setLifecycleEventsEnabled({enabled: true});

  await pageSession.protocol.Page.onceLifecycleEvent(
      event => event.params.name === 'load');

  const attachedToTab2 = (await
      target.attachToTarget({targetId: result.targetId, flatten: true})).result;
  const dp2 = browserSession.createChild(attachedToTab2.sessionId).protocol;
  dp2.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await dp2.Target.setDiscoverTargets({discover: true});

  browserSession.protocol.Target.setDiscoverTargets(
      {discover: true, filter: [{type: 'tab'}]});

  dp1.Target.onceAttachedToTarget().then(
      () => testRunner.log('FAIL: no new targets should be created'));
  dp2.Target.onceAttachedToTarget().then(
      () => testRunner.log('FAIL: no new targets should be created'));

  pageSession.evaluate(`window.close()`);
  await browserSession.protocol.Target.onceTargetDestroyed();

  testRunner.completeTest();
})

