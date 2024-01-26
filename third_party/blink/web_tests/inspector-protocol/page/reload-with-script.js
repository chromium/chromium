(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests scriptToEvaluateOnLoad passed to Page.reload is executed appopriately');

  dp.Runtime.enable(),
  dp.Runtime.onConsoleAPICalled(e => {
    testRunner.log(`PAGE: ${e.params.args[0].value}`);
  });
  dp.Page.enable(),
  dp.Page.reload({ scriptToEvaluateOnLoad: `console.log('reloading');` });
  await dp.Page.onceLoadEventFired();
  testRunner.log(`Reloaded with script`);
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();
  testRunner.log(`Reloaded without script`);

  testRunner.completeTest();
})
