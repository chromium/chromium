(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that calling WebAuthn.enable starts the WebAuthn virtual " +
          "authenticator environment.");

  await dp.WebAuthn.enable();

  const result = await session.evaluateAsync("registerCredential()");
  testRunner.log(result.status);
  testRunner.completeTest();
})
