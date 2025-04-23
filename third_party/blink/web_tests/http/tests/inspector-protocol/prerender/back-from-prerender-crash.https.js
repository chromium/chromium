(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(`Tests that we don't crash when navigating from pre-rendered page to BF-cached original page.`);

  const pageUrl = 'http://devtools.oopif-a.test:8000/inspector-protocol/prerender/resources/simple-prerender.html';

  const bp = testRunner.browserP();

  const params = {url: 'about:blank', forTab: true};
  const tabTargetId = (await bp.Target.createTarget(params)).result.targetId;
  const tabTargetSessionId =
      (await bp.Target.attachToTarget({targetId: tabTargetId, flatten: true}))
          .result.sessionId;
  const tabTargetSession =
      new TestRunner.Session(testRunner, tabTargetSessionId);
  const tp = tabTargetSession.protocol;

  const events = [];
  tp.Target.onAttachedToTarget(event => {
    events.push(event);
  });
  await tp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const session = tabTargetSession.createChild(events[0].params.sessionId);

  session.protocol.Page.enable();
  session.protocol.Preload.enable();

  const prerenderReady = session.protocol.Preload.oncePrerenderStatusUpdated(
      e => e.params.status == 'Ready');
  session.navigate(pageUrl);

  const attached = (await tp.Target.onceAttachedToTarget()).params;
  testRunner.log(attached);

  // Attach to prerender target and make sure it navigates where it should.
  const prerenderSession = testRunner.browserSession().createChild(attached.sessionId);
  prerenderSession.protocol.Page.enable();
  prerenderSession.protocol.Runtime.runIfWaitingForDebugger();
  const navigated = (await prerenderSession.protocol.Page.onceFrameNavigated()).params;
  testRunner.log(`${navigated.type}: ${navigated.frame.url}`);

  await prerenderReady;

  // Now activate prerender and make sure old target detaches.
  session.evaluate(`document.getElementById('link').click()`);
  await tp.Target.onceDetachedFromTarget();

  testRunner.log(`Navigated to ${await prerenderSession.evaluate('location.href')}`);
  prerenderSession.evaluate(`history.go(-1)`);
  await prerenderSession.protocol.Page.onceFrameNavigated();
  testRunner.log(`Now back to ${await prerenderSession.evaluate('location.href')}`);

  testRunner.completeTest();
});
