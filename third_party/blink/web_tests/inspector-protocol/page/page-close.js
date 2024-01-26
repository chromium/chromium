(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that page.requestClose method runs beforeunload hooks.');

  await dp.Runtime.enable();
  await session.evaluate(() => {
    window.addEventListener('beforeunload', function (event) {
      console.log('YES');
    }, false);
  });

  dp.Page.close();

  // Console message should be emitted from-inside the beforeunload handler.
  await dp.Runtime.onceConsoleAPICalled();
  testRunner.log('SUCCESS!');
  testRunner.completeTest();
})
