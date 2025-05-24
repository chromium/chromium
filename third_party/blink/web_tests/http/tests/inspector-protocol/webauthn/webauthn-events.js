(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const username1 = "reimu";
  const displayName1 = "Reimu Hakurei";
  const username2 = "marisa";
  const displayName2 = "Marisa Kirisame";

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
    user: {
      name: "${username1}",
      displayName: "${displayName1}",
      id: Uint8Array.from([1]),
    }
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
    testRunner.log("User name: " + event.params.credential.userName);
    testRunner.log("User display name: " + event.params.credential.userDisplayName);
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

  // Update the user details.
  const updateEvent = dp.WebAuthn.onceCredentialUpdated();
  const updateCredential = await session.evaluateAsync(`signalCurrentUserDetails({
    rpId: window.location.hostname,
    userId: "AQ",
    name: "${username2}",
    displayName: "${displayName2}",
  })`);
  testRunner.log("Updating user details: " + updateCredential.status);
  logEvent(await updateEvent);

  // Remove the credential.
  const deleteEvent = dp.WebAuthn.onceCredentialDeleted();
  const deleteCredential = await session.evaluateAsync(`signalUnknownCredential({
    rpId: window.location.hostname,
    credentialId: "${result.credential.id}",
  })`);
  testRunner.log("Removing credential: " + deleteCredential.status);
  const event = await deleteEvent;
  testRunner.log("Authenticator ID matches: " + (event.params.authenticatorId == authenticatorId));
  testRunner.log("Credential ID matches: " + (event.params.credentialId == credentialId));
  testRunner.completeTest();
})
