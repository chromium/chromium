(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session} = await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches to pages opened by click on noopener link.`);

  const target = testRunner.browserP().Target;
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await page.navigate('../resources/link.html');
  const linkSelectors = [
    'body > a[rel]',
    'body > a:not([rel])',
  ];
  for (const selector of linkSelectors) {
    testRunner.log('\nClicking on ' + selector);
    const attachedEventPromise = target.onceAttachedToTarget();
    session.evaluate(`document.querySelector('${selector}').click()`);
    const attachedEvent = await attachedEventPromise;
    testRunner.log('Attached to new window');

    const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
    const dp = popupSession.protocol;
    await Promise.all([
      dp.Emulation.setUserAgentOverride({ userAgent: 'Overridden value' }),
      dp.Page.enable(),
      dp.Page.setLifecycleEventsEnabled({enabled: true}),
      dp.Page.onceLifecycleEvent(event => event.params.name === 'load'),
      dp.Runtime.runIfWaitingForDebugger(),
    ]);
    testRunner.log('Resumed');
    const body = await popupSession.evaluate(`document.body.textContent`);
    testRunner.log('New window content:\n' + body);
  }

  testRunner.completeTest();
})
