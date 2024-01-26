(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests reloading pages with data URLs.');

  await dp.Page.enable();

  await dp.Page.navigate({url: 'data:text/html,hello!'});
  await session.evaluate(() => window.foo = 42);
  await dp.Page.reload();
  dp.Page.setLifecycleEventsEnabled({enabled: true});
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  testRunner.log('Querying window.foo after reload (expect "undefined"): ' + (await session.evaluate(() => window.foo)));
  testRunner.completeTest();
})
