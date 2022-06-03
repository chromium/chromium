(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that prerender gets the UA override.`);

  const target = testRunner.browserP().Target;
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await session.protocol.Emulation.setUserAgentOverride({userAgent: 'Lynx v0.1'});
  await session.navigate('resources/prerender-echo-header.html');

  const textContent = await session.evaluateAsync('gotMessage');
  const userAgent = textContent.split('\n').find(line => /^User-Agent:/.test(line));
  testRunner.log(`got: ${userAgent}`);
  testRunner.completeTest();
});
