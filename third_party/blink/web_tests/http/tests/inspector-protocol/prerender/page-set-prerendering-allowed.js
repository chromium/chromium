(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test that Page.setPrerenderingAllowed disables prerendering');

  await dp.Preload.enable();
  await dp.Page.enable();
  await dp.Page.setPrerenderingAllowed({isAllowed: false});

  page.navigate('resources/simple-prerender.html');

  const event = await dp.Preload.oncePrerenderStatusUpdated();
  testRunner.log(event, null, ['loaderId', 'sessionId']);

  testRunner.completeTest();
});
