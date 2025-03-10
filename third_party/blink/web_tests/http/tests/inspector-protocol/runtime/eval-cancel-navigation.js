(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'http://first.test:8000/inspector-protocol/resources/test-page.html',
    `Tests that client-side navigation can be cancelled.`);
  await dp.Page.enable();

  const navigate_to = 'http://second.test:8000/inspector-protocol/resources/test-page.html';
  session.evaluate(`
    location.href = '${navigate_to}';
    window.stop();
  `);

  await dp.Page.onceFrameStartedLoading();
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(await session.evaluate('location.href'));
  testRunner.completeTest();
})
