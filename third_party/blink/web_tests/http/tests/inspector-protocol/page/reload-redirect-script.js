(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Page.reload with scriptToEvaluateOnLoad drops the script on cross-origin redirects.`);

  const FetchHelper =
      await testRunner.loadScript('../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  helper.setEnableLogging(false);
  await helper.enable();

  await dp.Page.enable();

  // 1. Direct reload (no redirect)
  testRunner.log('\n--- Direct reload (no redirect) ---');
  helper.onceRequest(/a.test\/direct/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Direct</body></html>'),
  });
  await dp.Page.navigate({url: 'http://a.test/direct'});
  await dp.Page.onceLoadEventFired();

  helper.onceRequest(/a.test\/direct/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Direct Reloaded</body></html>'),
  });
  dp.Page.reload({scriptToEvaluateOnLoad: 'window.__injected = "direct";'});
  await dp.Page.onceLoadEventFired();
  const directInjected = await session.evaluate('window.__injected');
  testRunner.log(`window.__injected: ${directInjected}`);

  // 2. Same-origin redirect
  testRunner.log('\n--- Same-origin redirect ---');
  helper.onceRequest(/a.test\/same-origin-start/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Same Origin Start</body></html>'),
  });
  await dp.Page.navigate({url: 'http://a.test/same-origin-start'});
  await dp.Page.onceLoadEventFired();

  helper.onceRequest(/a.test\/same-origin-start/).fulfill({
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: 'http://a.test/same-origin-dest'},
    ],
  });
  helper.onceRequest(/a.test\/same-origin-dest/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Same Origin Dest</body></html>'),
  });
  dp.Page.reload(
      {scriptToEvaluateOnLoad: 'window.__injected = "same-origin";'});
  await dp.Page.onceLoadEventFired();
  const sameOriginInjected = await session.evaluate('window.__injected');
  testRunner.log(`window.__injected: ${sameOriginInjected}`);

  // 3. Cross-origin redirect
  testRunner.log('\n--- Cross-origin redirect ---');
  helper.onceRequest(/a.test\/cross-origin-start/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Cross Origin Start</body></html>'),
  });
  await dp.Page.navigate({url: 'http://a.test/cross-origin-start'});
  await dp.Page.onceLoadEventFired();

  helper.onceRequest(/a.test\/cross-origin-start/).fulfill({
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: 'http://b.test/cross-origin-dest'},
    ],
  });
  helper.onceRequest(/b.test\/cross-origin-dest/).fulfill({
    responseCode: 200,
    body: btoa('<html><head></head><body>Cross Origin Dest</body></html>'),
  });
  dp.Page.reload(
      {scriptToEvaluateOnLoad: 'window.__injected = "cross-origin";'});
  await dp.Page.onceLoadEventFired();
  const crossOriginInjected = await session.evaluate('window.__injected');
  testRunner.log(`window.__injected: ${crossOriginInjected}`);

  testRunner.completeTest();
})
