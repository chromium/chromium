(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that timezone can be overridden.');

  async function setTimezoneOverride(timezoneId) {
    await dp.Emulation.setTimezoneOverride({ timezoneId });
    const result = await dp.Runtime.evaluate(
        { expression: 'new Date(1557437122406).toString()' });
    return result.result.result.value;
  }

  // Use timezones that don't observe daylight savings.
  testRunner.log(await setTimezoneOverride(`America/Jamaica`));
  testRunner.log(await setTimezoneOverride(`Pacific/Honolulu`));
  testRunner.log(await setTimezoneOverride(`Europe/Moscow`));

  testRunner.completeTest();
})
