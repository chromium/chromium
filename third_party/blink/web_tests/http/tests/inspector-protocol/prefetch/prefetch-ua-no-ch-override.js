(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { page, dp, session } = await testRunner.startBlank(
      `Tests that prefetch gets the UA override without touching sec-ch.`);

  await dp.Preload.enable();
  await session.protocol.Emulation.setUserAgentOverride({
    userAgent: 'Lynx v0.1'
  });

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch-echo-header.html");
  await dp.Preload.oncePrefetchStatusUpdated(e => e.params.status === 'Ready');

  // Go to the prefetched page.
  await session.evaluate(`document.getElementById('link').click()`);
  const loadPromise = dp.Page.once('loadEventFired');
  let textContent = await session.evaluate(`document.body.textContent`);

  // Wait until header is printed.
  while (textContent === undefined) {
    textContent = await session.evaluate(`document.body.textContent`);
  }
  await loadPromise;

  testRunner.log(textContent);
  testRunner.completeTest();
});
