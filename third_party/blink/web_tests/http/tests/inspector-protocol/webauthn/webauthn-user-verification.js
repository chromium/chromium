(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addVirtualAuthenticator sets user verification");

  // Create an authenticator that supports user verification and fails the
  // check.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "nfc",
      hasResidentKey: false,
      hasUserVerification: true,
    },
  })).result.authenticatorId;
  await dp.WebAuthn.setUserVerified({authenticatorId, isUserVerified: false});

  // This should return a NotAllowedError.
  testRunner.log((await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      userVerification: "required"
    }
  })`)).status);

  // Instruct the authenticator to succeed the user verification.
  await dp.WebAuthn.setUserVerified({authenticatorId, isUserVerified: true});

  // This should succeed.
  testRunner.log((await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      userVerification: "required"
    }
  })`)).status);

  testRunner.completeTest();
})
