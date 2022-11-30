(async function(testRunner) {
  const pageUrl = 'http://devtools.oopif-a.test:8000/inspector-protocol/portals/resources/page-with-portal.html';
  const portalUrl = 'http://devtools.oopif-b.test:8000/inspector-protocol/portals/resources/portal.html';
  const {session, dp} = await testRunner.startURL(pageUrl, 'Tests how portal targets are auto-attached.');

  const tabs = (await dp.Target.getTargets({filter: [{type: "tab"}]})).result.targetInfos;
  const tabUnderTest = tabs.find(target => target.url === pageUrl);
  const tp = (await testRunner.browserSession().attachChild(tabUnderTest.targetId)).protocol;

  async function autoAttach(protocol) {
    const attachedTargets = [];
    const handleTarget = event => attachedTargets.push(event.params);
    const readyPromise = protocol.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
    protocol.Target.onAttachedToTarget(handleTarget);
    await readyPromise;
    protocol.Target.offAttachedToTarget(handleTarget);
    return attachedTargets.sort(
      (a, b) => `${a.targetInfo.type}\n${a.targetInfo.url}`.localeCompare(
        `${b.targetInfo.type}\n${b.targetInfo.url}`));
  }

  const tabTargets = await autoAttach(tp);
  testRunner.log(tabTargets, "Auto-attached via Tab target: ");
  testRunner.log(await autoAttach(dp), "Auto-attached via Page target (legacy mode): ");

  const pageSessionViaTab = tabTargets.find(event => event.targetInfo.url === pageUrl).sessionId;
  const dp2 = await testRunner.browserSession().createChild(pageSessionViaTab).protocol;
  testRunner.log(await autoAttach(dp2), "Auto-attached via Page target (Tab target mode): ");

  // Now activate portal and observe page target is gone.
  const detachedPromise = tp.Target.onceDetachedFromTarget();
  await session.evaluateAsync(`document.getElementsByTagName('portal')[0].activate()`);
  const detached = (await detachedPromise).params;
  const detachedTarget = tabTargets.find(target => target.targetInfo.targetId === detached.targetId);
  testRunner.log(`Detached from ${detachedTarget.targetInfo.url}`);

  const portalTarget = tabTargets.find(item => item.targetInfo.url === portalUrl);
  const portalSession = testRunner.browserSession().createChild(portalTarget.sessionId);
  testRunner.log(await portalSession.evaluate('window.portalStatus'), 'Portal target:');

  // Now attach and detach another portal to cover associated instrumentation signals.
  portalSession.evaluate(`
    const portal = document.createElement('portal');
    portal.src = '${portalUrl}';
    document.body.appendChild(portal);
  `);
  const portal2Attached = (await tp.Target.onceAttachedToTarget()).params.targetInfo;
  testRunner.log(portal2Attached);
  portalSession.evaluate(`
    document.getElementsByTagName('portal')[0].remove()
  `);
  const portal2Detached = (await tp.Target.onceDetachedFromTarget()).params;
  testRunner.log(`Detached, correct target: ${portal2Detached.targetId === portal2Attached.targetId}`);
  testRunner.completeTest();
})