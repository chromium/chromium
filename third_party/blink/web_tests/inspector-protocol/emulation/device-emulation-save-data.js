(async function(/** @type {import('test_runner').TestRunner} */testRunner) {
  const {page, session} = await testRunner.startBlank(
      'Tests that overriding saveData changes the navigator property');

  const defaultValue = await session.evaluate('navigator.connection.saveData');

  await session.protocol.Emulation.setDataSaverOverride({dataSaverEnabled: true});
  const enabled = await session.evaluate('navigator.connection.saveData');
  testRunner.log(`Enabled Value: ${enabled}`);

  await session.navigate('../resources/blank.html');
  const enabledAfterNavigation = await session.evaluate('navigator.connection.saveData');
  testRunner.log(`Enabled Value after navigation: ${enabledAfterNavigation}`);

  await session.protocol.Emulation.setDataSaverOverride({dataSaverEnabled: false});
  const disabled = await session.evaluate('navigator.connection.saveData');
  testRunner.log(`Disabled Value: ${disabled}`);

  await session.protocol.Emulation.setDataSaverOverride({dataSaverEnabled: true});
  const enabledAgain = await session.evaluate('navigator.connection.saveData');
  testRunner.log(`Enabled Again Value: ${enabledAgain}`);

  await session.protocol.Emulation.setDataSaverOverride({});
  const reset = await session.evaluate('navigator.connection.saveData');
  if(reset !== defaultValue)
    testRunner.log(`FAIL! Reset value ${reset} should equal default value ${defaultValue}`);

  testRunner.completeTest();
})
