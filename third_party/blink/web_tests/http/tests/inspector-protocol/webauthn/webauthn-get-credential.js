(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command getCredential works");

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

  // Register a resident credential.
  let result = (await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      requireResidentKey: true,
    },
  })`));
  testRunner.log(result.status);

  let credentialId =
      await session.evaluate(`base64urlToBase64("${result.credential.id}")`);

  // Get the registered credential.
  let credential =
      (await dp.WebAuthn.getCredential({authenticatorId, credentialId})).result.credential;

  testRunner.log("credentialId: " +
      (credential.credentialId == credentialId ? "matches" : "does not match"));
  testRunner.log("isResidentCredential: " + credential.isResidentCredential);
  testRunner.log("rpId: " + credential.rpId);
  testRunner.log("signCount: " + credential.signCount);
  testRunner.log("userHandle: " + credential.userHandle);
  testRunner.log("name: " + credential.userName);
  testRunner.log("displayName: " + credential.userDisplayName);

  // We should be able to parse the private key.
  let keyData =
      Uint8Array.from(atob(credential.privateKey), c => c.charCodeAt(0)).buffer;
  let key = await window.crypto.subtle.importKey(
      "pkcs8", keyData, { name: "ECDSA", namedCurve: "P-256" },
      true /* extractable */, ["sign"]);

  testRunner.log(key);

  testRunner.completeTest();
})
