(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command getCredentials works");

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

  // No credentials registered yet.
  testRunner.log(await dp.WebAuthn.getCredentials({authenticatorId}));

  // Register a non-resident credential.
  testRunner.log((await session.evaluateAsync("registerCredential()")).status);

  // Register a resident credential.
  testRunner.log((await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      requireResidentKey: true,
    },
  })`)).status);

  let logCredential = credential => {
    testRunner.log("isResidentCredential: " + credential.isResidentCredential);
    testRunner.log("signCount: " + credential.signCount);
    testRunner.log("rpId: " + credential.rpId);
    testRunner.log("userHandle: " + credential.userHandle);
  };
  // Get the registered credentials.
  let credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  let residentCredential = credentials.find(cred => cred.isResidentCredential);
  let nonResidentCredential = credentials.find(cred => !cred.isResidentCredential);
  testRunner.log("Resident Credential:");
  logCredential(residentCredential);
  testRunner.log("Non-Resident Credential:");
  logCredential(nonResidentCredential);

  // Authenticating with the non resident credential should succeed.
  testRunner.log(await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: base64ToArrayBuffer("${nonResidentCredential.credentialId}"),
    transports: ["usb", "ble", "nfc"],
  })`));

  // Sign count should be increased by one for |nonResidentCredential|.
  credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(credentials.find(
      cred => cred.credentialId === nonResidentCredential.credentialId).signCount);

  // We should be able to parse the private key.
  let keyData =
      Uint8Array.from(atob(nonResidentCredential.privateKey), c => c.charCodeAt(0)).buffer;
  let key = await window.crypto.subtle.importKey(
      "pkcs8", keyData, { name: "ECDSA", namedCurve: "P-256" },
      true /* extractable */, ["sign"]);

  testRunner.log(key);

  testRunner.completeTest();
})
