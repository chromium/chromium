(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addVirtualAuthenticator works");

  // Create an CTAP2 NFC authenticator and verify it is the one responding to
  // navigator.credentials.create().
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "nfc",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  let result = await session.evaluateAsync("registerCredential()");
  testRunner.log(result.status);
  testRunner.log(result.credential.transports);

  // Try with a BLE U2F authenticator.
  await dp.WebAuthn.removeVirtualAuthenticator({ authenticatorId });
  await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "u2f",
      transport: "ble",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  result = await session.evaluateAsync("registerCredential()");
  testRunner.log(result.status);
  testRunner.log(result.credential.transports);

  testRunner.completeTest();
})
