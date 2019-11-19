(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addVirtualAuthenticator simulates user presence");

  // Create an authenticator that will not simulate user presence.
  await dp.WebAuthn.enable();
  await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "nfc",
      hasResidentKey: false,
      hasUserVerification: false,
      automaticPresenceSimulation: false,
    },
  });

  // Registering a credential should wait until an authenticator that verifies
  // user presence is plugged.
  session.evaluateAsync("registerCredential()").then(result => {
    testRunner.log(result.status);
    testRunner.log(result.credential.transports);
    testRunner.completeTest();
  });

  // Plug in an authenticator that does simulate user presence. The credential
  // creation promise should resolve with transport == ble then.
  await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "u2f",
      transport: "ble",
      hasResidentKey: false,
      hasUserVerification: false,
      automaticPresenceSimulation: true,
    },
  });
})
