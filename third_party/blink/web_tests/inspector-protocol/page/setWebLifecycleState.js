(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that page.setWebLifecycleState method invokes the state transition callbacks.');

  await dp.Runtime.enable();
  await session.evaluate(() => {
    window.document.addEventListener('freeze', function (event) {
      console.log('FROZEN');
    }, false);
    window.document.addEventListener('resume', function (event) {
      console.log('RESUMED');
    }, false);
  });

  dp.Page.setWebLifecycleState({state: 'frozen'});
  // Console message should be emitted from-inside the onfreeze handler.
  await dp.Runtime.onceConsoleAPICalled(messageObject => messageObject.params.args[0].value === 'FROZEN');

  dp.Page.setWebLifecycleState({state: 'active'});
  // Console message should be emitted from-inside the onresume handler.
  await dp.Runtime.onceConsoleAPICalled(messageObject => messageObject.params.args[0].value === 'RESUMED');

  var message = await dp.Page.setWebLifecycleState({state: 'abc'});
  if (!message.error) {
    testRunner.log('Failed to err on an invalid state');
    testRunner.completeTest();
    return;
  }

  testRunner.log('SUCCESS!');
  testRunner.completeTest();
})
