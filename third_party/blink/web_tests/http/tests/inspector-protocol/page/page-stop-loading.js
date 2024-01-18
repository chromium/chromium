(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.stopLoading cancels navigation');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Network.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});
  const navigatePromise = dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html')});
  dp.Page.stopLoading();
  await navigatePromise;
  testRunner.log('navigation finished');
  testRunner.completeTest();
})
