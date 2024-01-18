(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() supports only flatten protocol.`);

  const target = testRunner.browserP().Target;
  const response = await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: false});
  testRunner.log(response, 'Tried to auto-attach with not flatten protocol');

  testRunner.completeTest();
})
