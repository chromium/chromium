(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.navigateWithinDocument is issued for history API and anchor navigation.`);

  await dp.Page.enable();
  await dp.Runtime.enable();

  testRunner.log('-- Test Page.navigate() to anchor URL --');
  await dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#foo')});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.log('-- Test Page.navigate() to another anchor URL --');
  await dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#bar')});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.log('-- Test history.pushState() --');
  dp.Runtime.evaluate({ expression: `history.pushState({}, '', 'wow.html')`});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.log('-- Test history.replaceState() --');
  dp.Runtime.evaluate({ expression: `history.replaceState({}, '', '/replaced.html')`});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.log('-- Test history.back() --');
  dp.Runtime.evaluate({ expression: `history.back()`});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.log('-- Test history.forward() --');
  dp.Runtime.evaluate({ expression: `history.forward()`});
  testRunner.log(await dp.Page.onceNavigatedWithinDocument());

  testRunner.completeTest();
})
