(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

  var events = [];
  dp.Page.onLifecycleEvent(event => {
    events.push(event);
    if (event.params.name === 'networkIdle') {
      var names = events.map(event => event.params.name);
      names.sort();
      testRunner.log(names);
      testRunner.completeTest();
    }
  });

  var response = await dp.Page.navigate({url: "data:text/html,Hello!"});
})
