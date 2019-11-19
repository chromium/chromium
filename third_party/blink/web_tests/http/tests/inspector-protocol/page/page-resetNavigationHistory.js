(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests Page.resetNavigationHistory');
  await dp.Runtime.evaluate({expression: `history.pushState({}, '', window.location.href + '&foo')`});
  await dp.Runtime.evaluate({expression: `history.pushState({}, '', window.location.href + '&bar')`});

  let length = await session.evaluate(`history.length`);
  testRunner.log('Length before reset: ' + length);

  await dp.Page.resetNavigationHistory();
  length = await session.evaluate(`history.length`);
  testRunner.log('Length after reset: ' + length);

  testRunner.completeTest();
})
