(async function(testRunner) {
  testRunner.log('Tests that device emulation affects portals.');

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const browserSessionId = (await target.attachToBrowserTarget()).result.sessionId;

  testRunner.log('Loading portal host');
  const browserSession = new TestRunner.Session(testRunner, browserSessionId);
  const attachedEvent = target.onceAttachedToTarget();
  const portalHostUrl = testRunner.url('http://devtools.oopif-a.test:8000/inspector-protocol/resources/test-page.html');
  browserSession.protocol.Target.createTarget({url: portalHostUrl});
  const session = new TestRunner.Session(testRunner, (await attachedEvent).params.sessionId);
  await session.protocol.Runtime.enable();
  testRunner.log('Location: ' + await session.evaluate('location.href'));

  testRunner.log('Creating portal');
  const portalAttachedEvent = target.onceAttachedToTarget();
  await session.evaluate(`
    var portal = document.createElement('portal');
    portal.src = 'http://devtools.oopif-b.test:8000/inspector-protocol/resources/test-page.html';
    document.body.appendChild(portal);
  `);
  const portalSession = new TestRunner.Session(testRunner, (await portalAttachedEvent).params.sessionId);
  await portalSession.protocol.Runtime.runIfWaitingForDebugger();
  await portalSession.protocol.Runtime.enable();
  testRunner.log('Portal location: ' + await portalSession.evaluate('location.href'));

  const dumpMetrics = `
    function dumpMetrics() {
      let results = [];
      results.push('width             = ' + screen.width);
      results.push('height            = ' + screen.height);
      results.push('orientation type  = ' + screen.orientation.type);
      results.push('orientation angle = ' + screen.orientation.angle);
      return results.join('\\n');
    };
  `;
  await session.evaluate(dumpMetrics);
  await portalSession.evaluate(dumpMetrics);

  await session.evaluate('testRunner.disableMockScreenOrientation()');
  await portalSession.evaluate('testRunner.disableMockScreenOrientation()');
  await session.protocol.Emulation.clearDeviceMetricsOverride();
  await session.protocol.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1.0,
    mobile: true,
    screenOrientation: {type: 'landscapeSecondary', angle: 270},
  });
  testRunner.log('Metrics:');
  let metrics = await session.evaluate('dumpMetrics()');
  testRunner.log(metrics);
  testRunner.log('Portal metrics:');
  let portalMetrics = await portalSession.evaluate('dumpMetrics()');
  testRunner.log(portalMetrics);

  await session.protocol.Emulation.clearDeviceMetricsOverride();
  await session.protocol.Emulation.setDeviceMetricsOverride({
    width: 500,
    height: 200,
    deviceScaleFactor: 2.0,
    mobile: true,
    screenOrientation: {type: 'portraitPrimary', angle: 0}
  });
  testRunner.log('Metrics:');
  metrics = await session.evaluate('dumpMetrics()');
  testRunner.log(metrics);
  testRunner.log('Portal metrics:');
  portalMetrics = await portalSession.evaluate('dumpMetrics()');
  testRunner.log(portalMetrics);

  await browserSession.disconnect();
  testRunner.completeTest();
})
