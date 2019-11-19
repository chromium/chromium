(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addCredential works");

  // Create an authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // Register a non-resident credential.
  const nonResidentCredentialId = "cred-1";
  testRunner.log(await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(nonResidentCredentialId),
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
      isResidentCredential: false,
    }
  }));

  // Authenticate with the non-resident credential.
  testRunner.log(await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${nonResidentCredentialId}"),
    transports: ["usb", "ble", "nfc"],
  })`));

  // Register a resident credential.
  const userHandle = "nina";
  const residentCredentialId = "cred-2";
  testRunner.log(await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(residentCredentialId),
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
      isResidentCredential: true,
      userHandle: btoa(userHandle),
    }
  }));

  // Authenticate with the resident credential.
  testRunner.log(await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${residentCredentialId}"),
    transports: ["usb", "ble", "nfc"],
  })`));

  testRunner.completeTest();
})
