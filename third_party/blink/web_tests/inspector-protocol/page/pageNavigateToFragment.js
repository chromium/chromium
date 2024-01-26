(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Page.navigate returns for fragment navigation.');

  await dp.Page.enable();
  let result = await dp.Page.navigate({url: testRunner.url('../resources/blank.html')});
  testRunner.log(result);
  await dp.Page.onceLoadEventFired();
  result = await dp.Page.navigate({url: testRunner.url('../resources/blank.html#fragment')});
  testRunner.log(result);

  testRunner.completeTest();
})
