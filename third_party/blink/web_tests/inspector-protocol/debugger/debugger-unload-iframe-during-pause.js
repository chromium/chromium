(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that we can unload an iframe during a pause');

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({enabled: true});

  testRunner.log('Navigating to url');

  await dp.Page.navigate(
      {url: testRunner.url('resources/delete-iframe-nodelist.html')});
  // Triggers debugger;

  const message = await dp.Debugger.oncePaused();
  testRunner.log('Paused');

  // Serialize variables to cause the length getter to unload the iframe.
  var scopeChain = message.params.callFrames[0].scopeChain;
  var localScopeObjectIds = [];
  for (var scope of scopeChain) {
    if (scope.type === 'local')
      localScopeObjectIds.push(scope.object.objectId);
  }

  for (var objectId of localScopeObjectIds) {
    const {result, error} = await dp.Runtime.getProperties({objectId});
    testRunner.log(result ?? error);
  }

  testRunner.log('Resuming');
  await dp.Debugger.resume();
  await dp.Debugger.onceResumed();
  testRunner.log('Resumed');

  testRunner.completeTest();
})
