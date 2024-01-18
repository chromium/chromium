(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session} = await testRunner.startBlank(
      'Tests that overriding hardware concurrency changes the navigator property');

  const defaultValue = await session.evaluate('navigator.hardwareConcurrency');

  await session.protocol.Emulation.setHardwareConcurrencyOverride(
      {hardwareConcurrency: defaultValue + 1});
  const plusOne = await session.evaluate('navigator.hardwareConcurrency');
  testRunner.log(`Concurrency, plus one: ${plusOne - defaultValue}`);

  session.evaluate('window.reload()');
  await session.protocol.Emulation.setHardwareConcurrencyOverride(
      {hardwareConcurrency: defaultValue + 1});
  const stillPlusOne = await session.evaluate('navigator.hardwareConcurrency');
  testRunner.log(`Concurrency, after reload: ${stillPlusOne - defaultValue}`);

  await session.protocol.Emulation.setHardwareConcurrencyOverride(
      {hardwareConcurrency: 0});
  const zero = await session.evaluate('navigator.hardwareConcurrency');
  testRunner.log(`Concurrency, can't set to zero: ${zero - defaultValue}`);

  await session.protocol.Emulation.setHardwareConcurrencyOverride(
      {hardwareConcurrency: -1});
  const negative = await session.evaluate('navigator.hardwareConcurrency');
  testRunner.log(
      `Concurrency, can't set to negative: ${negative - defaultValue}`);

  await session.protocol.Emulation.setHardwareConcurrencyOverride(
      {hardwareConcurrency: defaultValue});
  const resetValue = await session.evaluate('navigator.hardwareConcurrency');
  testRunner.log(`Concurrency, reset to default: ${resetValue - defaultValue}`);

  testRunner.completeTest();
})
