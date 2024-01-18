(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      `Tests that prerender gets the UA override.`);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Preload.enable();

  await session1.protocol.Emulation.setUserAgentOverride({
    userAgent: 'Lynx v0.1',
    userAgentMetadata: {
      platform: 'Lynx',
      platformVersion: '0.1',
      architecture: '',
      model: 'foobar',
      mobile: true
    }
  });
  session1.navigate('resources/prerender-echo-header.html');
  await session1.protocol.Preload.oncePrerenderStatusUpdated(e => e.params.status === 'Ready');

  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  const textContent = await session2.evaluate('document.body.textContent');
  const userAgent = textContent.split('\n').find(line => /^User-Agent:/.test(line));
  testRunner.log(`got: ${userAgent}`);
  const UAPlatform =
      textContent.split('\n').find(line => /^sec-ch-ua-platform:/.test(line));
  testRunner.log(`got: ${UAPlatform}`);
  const chMobile =
      textContent.split('\n').find(line => /^sec-ch-ua-mobile:/.test(line));
  testRunner.log(`got: ${chMobile}`);

  // Activate prerendered page.
  session1.evaluate(`document.getElementById('link').click()`);

  // Success
  const resultSuccess = await dp2.Preload.oncePrerenderStatusUpdated();
  testRunner.log(resultSuccess, '', ['loaderId', 'sessionId']);
  testRunner.completeTest();
});
