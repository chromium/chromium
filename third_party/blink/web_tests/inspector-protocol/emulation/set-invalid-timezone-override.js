(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests invalid timezone override handling.');

  async function setTimezoneOverride(timezoneId) {
    const result = await dp.Emulation.setTimezoneOverride({ timezoneId });
    return result.error;
  }

  testRunner.log(await setTimezoneOverride(`Foo/Bar`));

  testRunner.completeTest();
})
