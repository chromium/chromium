(async function(testRunner) {
  const portalUrl = 'http://devtools.oopif-b.test:8000/inspector-protocol/portals/resources/portal.html';
  const {session, dp} = await testRunner.startBlank('Tests that no spurious targetInfoChanged events are emitted during portal creation.');

  const tabs = (await dp.Target.getTargets({filter: [{type: "tab"}]})).result.targetInfos;
  const tabUnderTest = tabs.find(target => target.url.endsWith("/inspector-protocol-page.html"));
  const tp = (await testRunner.browserSession().attachChild(tabUnderTest.targetId)).protocol;

  await tp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});

  tp.Target.onTargetInfoChanged(event => testRunner.log(event.params, "FAILED: unexpected event"));

  await session.evaluateAsync(`
    new Promise(resolve => {
      const portal = document.createElement('portal');
      portal.src = '${portalUrl}';
      portal.onload = resolve;
      document.body.appendChild(portal);
    })
  `);
  testRunner.completeTest();
})