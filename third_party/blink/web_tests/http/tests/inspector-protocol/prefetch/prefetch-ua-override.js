(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { page, dp, session } = await testRunner.startBlank(
      `Tests that prefetch gets the UA override.`);

  await dp.Preload.enable();
  await session.protocol.Emulation.setUserAgentOverride({
    userAgent: 'Lynx v0.1',
    userAgentMetadata: {
      platform: 'Lynx',
      platformVersion: '0.1',
      architecture: '',
      model: 'foobar',
      mobile: true
    }
  });

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch-echo-header.html");
  await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === 'Ready');

  // Go to the prefetched page.
  await session.evaluate(`document.getElementById('link').click()`);
  let textContent = await session.evaluate(`document.body.textContent`);

  // Wait until header is printed.
  while (textContent === undefined) {
    textContent = await session.evaluate(`document.body.textContent`);
  }

  testRunner.log(textContent);
  testRunner.completeTest();
});
