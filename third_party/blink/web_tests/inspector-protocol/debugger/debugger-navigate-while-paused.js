(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that we can navigate away from paused page.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({enabled: true});

  // Note that evaluate will return an error after navigation.
  dp.Runtime.evaluate({expression: `debugger;`});
  await dp.Debugger.oncePaused(),
  testRunner.log('SUCCESS: Paused');

  await dp.Page.navigate({url: 'about:blank'});
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  testRunner.log('SUCCESS: navigated from paused page.');
  testRunner.completeTest();
})
