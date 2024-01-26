(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that default execution context accessed without enabling Runtime domain gets properly cleaned up on reload.`);
  await session.evaluate('window.dummyObject = { a : 1 };');
  var result = await dp.Runtime.evaluate({expression: 'window.dummyObject' });
  dp.Page.enable();
  dp.Page.reload();
  await dp.Page.onceLoadEventFired();
  testRunner.log(await dp.Runtime.getProperties({ objectId: result.result.result.objectId, ownProperties: true }));
  testRunner.completeTest();
})
