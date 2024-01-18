(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { session, dp } = await testRunner.startBlank(
    'Testing that automation emulation sets the webdriver flag');

  testRunner.log('Initial value: ' + await session.evaluate('navigator.webdriver'));
  await dp.Emulation.setAutomationOverride({ enabled: true });
  testRunner.log('Emulated value: ' + await session.evaluate('navigator.webdriver'));
  await dp.Emulation.setAutomationOverride({ enabled: false });
  testRunner.log('Original value: ' + await session.evaluate('navigator.webdriver'));

  testRunner.log('Navigation:');
  testRunner.log('  Initial value: ' + await session.evaluate('navigator.webdriver'));
  await dp.Emulation.setAutomationOverride({ enabled: true });
  await session.navigate("../resources/set-automation-override.html");
  testRunner.log('  Emulated value: ' + await session.evaluate('navigator.webdriver'));
  await dp.Emulation.setAutomationOverride({ enabled: false });
  testRunner.log('  Original value: ' + await session.evaluate('navigator.webdriver'));

  testRunner.log('IFrame:');
  async function evaluateIFrameWebdriver() {
    return await session.evaluate(`
      document.getElementById('test-iframe').contentWindow.navigator.webdriver
    `);
  }
  testRunner.log('  Initial value: ' + await evaluateIFrameWebdriver());
  await dp.Emulation.setAutomationOverride({ enabled: true });
  testRunner.log('  Emulated value: ' + await evaluateIFrameWebdriver());
  await dp.Emulation.setAutomationOverride({ enabled: false });
  testRunner.log('  Original value: ' + await evaluateIFrameWebdriver());

  testRunner.completeTest();
})
