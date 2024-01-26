(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that passing initial virtual time works.');

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause', initialVirtualTime: 1234567890});
  let now = await session.evaluate(`Date.now()`);
  testRunner.log(now);
  now = await session.evaluate(`Date.now()`);
  testRunner.log(now);
  now = await session.evaluate(`Date.now()`);
  testRunner.log(now);
  testRunner.completeTest();
})
