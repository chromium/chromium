(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command clearCredentials works");

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // Registering a credential for the first time should succeed.
  let result = await session.evaluateAsync("registerCredential()");
  testRunner.log(result.status);

  const registerCredentialScript = `
    registerCredential({
      excludeCredentials: [{
        type: "public-key",
        id: Uint8Array.from([${result.credential.rawId}]),
        transports: ["usb"]
      }],
    })`;

  // Trying to register it again should fail.
  result = await session.evaluateAsync(registerCredentialScript);
  testRunner.log(result.status);

  // After clearing the credentials, registering it again should succeed.
  await dp.WebAuthn.clearCredentials({authenticatorId});
  result = await session.evaluateAsync(registerCredentialScript);

  testRunner.log(result.status);

  testRunner.completeTest();
})
