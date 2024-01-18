(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that targetInfoChanged is sent for navigations.');

  await dp.Target.setDiscoverTargets({discover: true});
  dp.Page.navigate({url: 'data:text/plain,hello'});
  var event = await dp.Target.onceTargetInfoChanged();
  testRunner.log('TargetInfoChanged with url: ' + event.params.targetInfo.url);
  testRunner.completeTest();
})
