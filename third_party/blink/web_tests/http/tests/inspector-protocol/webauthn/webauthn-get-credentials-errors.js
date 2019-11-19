(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the WebAuthn command getCredentials validates parameters");

  // Try without enabling the WebAuthn environment.
  testRunner.log(await dp.WebAuthn.getCredentials({
    authenticatorId: "nonsense"
  }));

  // Try for an authenticator that does not exist.
  await dp.WebAuthn.enable();
  testRunner.log(await dp.WebAuthn.getCredentials({
    authenticatorId: "nonsense"
  }));

  testRunner.completeTest();
})
