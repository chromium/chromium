(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that prerender targets get auto-attached properly.`);

  const pageUrl = 'http://devtools.oopif-a.test:8000/inspector-protocol/prerender/resources/simple-prerender.html';

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const tabs = (await bp.Target.getTargets({filter: [{type: "tab"}]})).result.targetInfos;

  const tabUnderTest = tabs.filter(target => target.url.endsWith('/inspector-protocol-page.html'))[0];
  const tp = (await testRunner.browserSession().attachChild(tabUnderTest.targetId)).protocol;

  const autoAttachDone = tp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const targets = [];
  tp.Target.onAttachedToTarget(event => { targets.push(event.params.targetInfo); });
  await autoAttachDone;

  const navigateDone = session.navigate(pageUrl);

  const attached = (await tp.Target.onceAttachedToTarget()).params;
  testRunner.log(attached);

  // Attach to prerender target and make sure it navigates where it should.
  const prerenderSession = testRunner.browserSession().createChild(attached.sessionId);
  prerenderSession.protocol.Network.enable();
  prerenderSession.protocol.Page.enable();
  prerenderSession.protocol.Runtime.runIfWaitingForDebugger();
  const navigated = (await prerenderSession.protocol.Page.onceFrameNavigated()).params;
  testRunner.log(`${navigated.type}: ${navigated.frame.url}`);

  await navigateDone;

  tp.Target.onTargetInfoChanged(event => testRunner.log(event.params));

  // Now activate prerender and make sure old target detaches.
  session.evaluate(`document.getElementById('link').click()`);
  const detached = (await tp.Target.onceDetachedFromTarget()).params;
  testRunner.log(`Detached from ${targets[0].targetId === detached.targetId ? 'correct' : 'incorrect'} target`);

  const responseReceived = prerenderSession.protocol.Network.onceResponseReceived();

  // Make sure we're not accidentally passing an empty frameId.
  const frameId = attached.targetInfo.targetId || "<invalid>";
  const navigated2 = (await prerenderSession.protocol.Page.navigate({frameId, url: pageUrl}))?.result;
  testRunner.log('Correct frameId in Page.navigate response: ' +
     (navigated2?.frameId === frameId));
  // Do not await if navigation fails, so that we don't end up with a timeout if test fails.
  if (navigated2) {
    testRunner.log('Correct frameId in Network.responseReceived: ' +
        ((await responseReceived).params.frameId === frameId));
  }

  testRunner.completeTest();
});
