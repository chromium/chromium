(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that reloading a page with its own logs retains the logs.');

  dp.Runtime.enable();
  const firstLogPromise = dp.Runtime.onceConsoleAPICalled();
  dp.Page.enable();

  testRunner.log('Initial load:');
  await dp.Page.navigate({url: testRunner.url('../resources/console-log.html')});
  let event = await firstLogPromise;
  testRunner.log(`PAGE: ${event.params.args[0].value}`);

  const secondLogPromise = dp.Runtime.onceConsoleAPICalled();
  testRunner.log('Reloading page:');
  dp.Page.reload();
  event = await secondLogPromise;
  testRunner.log(`PAGE: ${event.params.args[0].value}`);

  testRunner.completeTest();
})
