(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that WebAuthn credBlob operations work");

  // Create an authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      ctap2Version: "ctap2_1",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      hasCredBlob: true,
      isUserVerified: true,
    },
  })).result.authenticatorId;

  // Register a credential with a credBlob through webauthn to confirm that
  // credBlob support was enabled.
  let result = await session.evaluateAsync(`registerCredential({
    extensions: {
      credBlob: new TextEncoder().encode("testing"),
    },
    authenticatorSelection: {
      requireResidentKey: true,
    },
  })`);
  testRunner.log(`Create credential result: ${result.status}`);
  testRunner.log(`credBlob result: ${result.credBlob}`);

  testRunner.completeTest();
})
