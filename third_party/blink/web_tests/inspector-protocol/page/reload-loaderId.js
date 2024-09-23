(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that reloading pages respects loaderId.');

  await dp.Page.enable();

  await dp.Page.navigate({url: 'data:text/html,hello!'});
  const {result: {frameTree: {frame: {loaderId}}}} =
      await dp.Page.getFrameTree();
  testRunner.log(`loaderId: ${loaderId ? '<string>' : 'undefined'}`);
  await session.evaluate(() => window.foo = 42);
  const loadEvent1 = dp.Page.onceLoadEventFired();
  await dp.Page.reload({loaderId});
  await loadEvent1;

  testRunner.log(
      'Querying window.foo after reload (expect "undefined"): ' +
      (await session.evaluate(() => window.foo)));

  testRunner.log('Now trying a reload with an incorrect loaderId');
  testRunner.log(await dp.Page.reload({loaderId: 'abcd'}));

  const loadEvent2 = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({url: 'data:text/html,hello!'});
  await loadEvent2;
  testRunner.log('Now trying a reload with an outdated loaderId');
  testRunner.log(await dp.Page.reload({loaderId}));
  testRunner.completeTest();
})
