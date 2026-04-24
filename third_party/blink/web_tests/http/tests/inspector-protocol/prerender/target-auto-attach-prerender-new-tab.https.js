(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(`Tests that new-tab prerender targets (target_hint="_blank") get auto-attached properly.`);

  const pageUrl = 'http://devtools.oopif-a.test:8000/inspector-protocol/prerender/resources/simple-prerender-with-target-hint.html';

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
  // Set up the attach promise before navigation to avoid missing the event.
  const attachedPromise = tp.Target.onceAttachedToTarget();
  const navigateDone = session.navigate(pageUrl);

  // The new-tab prerender target should be auto-attached to the tab target
  // with subtype "prerender" and type "page".
  const attached = (await attachedPromise).params;
  testRunner.log(attached);

  // Attach to prerender target and make sure it navigates where it should.
  const prerenderSession = testRunner.browserSession().createChild(attached.sessionId);
  prerenderSession.protocol.Network.enable();
  prerenderSession.protocol.Page.enable();
  prerenderSession.protocol.Runtime.runIfWaitingForDebugger();
  const navigated = (await prerenderSession.protocol.Page.onceFrameNavigated()).params;
  testRunner.log(`${navigated.type}: ${navigated.frame.url}`);

  await navigateDone;
  await prerenderReady;

  testRunner.completeTest();
});
