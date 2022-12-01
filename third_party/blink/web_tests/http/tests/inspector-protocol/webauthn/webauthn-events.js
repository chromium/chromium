(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that WebAuthn events work");

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      isUserVerified: true,
    },
  })).result.authenticatorId;

  // Register a credential.
  const credentialAddedEvent = dp.WebAuthn.onceCredentialAdded();
  const result = await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      requireResidentKey: true,
    },
  })`);
  testRunner.log("Registering a credential: " + result.status);
  const credentialId = await session.evaluate(`base64urlToBase64("${result.credential.id}")`);

  // Use devtools to inspect the generated credential and obtain the private
  // key.
  const privateKey = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials[0].privateKey;
  function logEvent(event) {
    testRunner.log("Authenticator ID matches: " + (event.params.authenticatorId == authenticatorId));
    testRunner.log("Credential ID matches: " + (event.params.credential.credentialId == credentialId));
    testRunner.log("Private key matches: " + (event.params.credential.privateKey == privateKey));
    testRunner.log("Resident credential: " + event.params.credential.isResidentCredential);
    testRunner.log("RP ID: " + event.params.credential.rpId);
    testRunner.log("Sign count: " + event.params.credential.signCount);
    testRunner.log("User handle: " + event.params.credential.userHandle);
  };

  // Wait for a credential added event.
  logEvent(await credentialAddedEvent);

  // Get an assertion.
  const assertionEvent = dp.WebAuthn.onceCredentialAsserted();
  const getCredential = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: base64ToArrayBuffer("${credentialId}"),
  })`);
  testRunner.log("Getting an assertion: " + getCredential.status);
  logEvent(await assertionEvent);
  testRunner.completeTest();
})
