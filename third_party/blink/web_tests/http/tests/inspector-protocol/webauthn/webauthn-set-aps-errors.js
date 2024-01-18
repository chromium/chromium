(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that the WebAuthn command setAutomaticPresenceSimulation validates parameters');

  // Try without enabling the WebAuthn environment.
  testRunner.log(await dp.WebAuthn.setAutomaticPresenceSimulation(
      {authenticatorId: 'nonsense', enabled: false}));

  // Try for an authenticator that does not exist.
  await dp.WebAuthn.enable();
  testRunner.log(await dp.WebAuthn.setAutomaticPresenceSimulation(
      {authenticatorId: 'nonsense', enabled: false}));

  testRunner.completeTest();
})
