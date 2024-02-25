(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addCredential validates parameters");

  const credentialId = "cred-1";
  const credentialOptions = {
    authenticatorId: "non-existant authenticator",
    credential: {
      credentialId: btoa(credentialId),
      privateKey: btoa("invalid private key"),
      signCount: 0,
      isResidentCredential: true,
    }
  };

  // Try without enabling the WebAuthn environment.
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try for an authenticator that does not exist.
  await dp.WebAuthn.enable();
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try without an RP ID.
  credentialOptions.authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try registering a resident credential on an authenticator not capable of
  // resident credentials.
  credentialOptions.credential.rpId = "devtools.test";
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try registering a resident credential without a user handle.
  credentialOptions.authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
    },
  })).result.authenticatorId;
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try a user handle that exceeds the max size.
  const MAX_USER_HANDLE_SIZE = 64;
  const longHandle = "a".repeat(MAX_USER_HANDLE_SIZE + 1);
  credentialOptions.credential.userHandle = btoa(longHandle);
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try with a private key that is not valid.
  credentialOptions.credential.userHandle = btoa("nina");
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try with a large blob on a non resident credential.
  credentialOptions.credential.privateKey =
      await session.evaluateAsync("generateBase64Key()");
  credentialOptions.credential.largeBlob = btoa("large blob");
  credentialOptions.credential.isResidentCredential = false;
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  testRunner.completeTest();
})
