(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = `http://devtools.test:8000/inspector-protocol/resources/empty.html`;
  const crossProcessURL = `https://devtools.test:8443/inspector-protocol/resources/empty.html`;
  const {page, session, dp} = await testRunner.startURL(pageURL,
      `Tests that the same timezone emulated in a page and popup is retained after cross-process popup navigation`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  // For test reliability, use timezone that does not observe daylight savings.
  const SINGLE_TIMEZONE = 'Pacific/Honolulu' ;
  await session.protocol.Emulation.setTimezoneOverride({timezoneId: SINGLE_TIMEZONE});
  testRunner.log(`main page time: ${await getTime(session)}`);

  // Opening popup with the same URL => must share a process.
  session.evaluate(`
    window.myWindow = window.open('${pageURL}');
    undefined;
  `);
  testRunner.log('Opened the window');
  const attachedEvent = await target.onceAttachedToTarget();
  const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  popupSession.protocol.Page.enable();
  popupSession.protocol.Runtime.runIfWaitingForDebugger();
  await popupSession.protocol.Page.onceLoadEventFired();

  // Once new popup loaded, emulate timezone in the popup. Make sure emulation works.
  await popupSession.protocol.Emulation.setTimezoneOverride({timezoneId: SINGLE_TIMEZONE});
  testRunner.log(`popup time before cross-process navigation: ${await getTime(popupSession)}`);

  // Do a cross-process navigation for a popup.
  await popupSession.navigate(crossProcessURL);
  testRunner.log(`popup time  after cross-process navigation: ${await getTime(popupSession)}`);

  testRunner.completeTest();

  async function getTime(session) {
    const result = await session.protocol.Runtime.evaluate(
          { expression: 'new Date(1557437122406).toString()' });
    return result.result.result.value;
  }
})
