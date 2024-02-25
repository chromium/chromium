(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that WebAuthn large blob operations work");

  // Create an authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      ctap2Version: "ctap2_1",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      hasLargeBlob: true,
      isUserVerified: true,
    },
  })).result.authenticatorId;

  // Register a credential with a large blob through webauthn.
  let result = await session.evaluateAsync(`registerCredential({
    extensions: {
      largeBlob: {
        support: "preferred",
      },
    },
    authenticatorSelection: {
      requireResidentKey: true,
    },
  })`);
  testRunner.log(`Create credential result: ${result.status}`);
  testRunner.log(`Large blob support: ${result.largeBlobSupported}`);

  // Register a credential with a large blob through devtools.
  const credentialId = btoa("cred-1");
  const largeBlob =
      "I'm Commander Shepard, and this is my favorite blob on the Citadel!";
  testRunner.log(await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId,
      userHandle: btoa("isabelle"),
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
      isResidentCredential: true,
      largeBlob: btoa(largeBlob),
    }
  }));

  // Read the large blob through the WebAuthn API.
  result = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("cred-1"),
    transports: ["usb", "ble", "nfc"],
  }, {
    extensions: {
      largeBlob: {
        read: true,
      },
    },
  })`);
  testRunner.log(`Assertion result: ${result.status}`);
  testRunner.log(`Got ${result.blob}`);

  // Read the large blob through Devtools.
  let credential =
      (await dp.WebAuthn.getCredential({authenticatorId, credentialId})).result.credential;
  testRunner.log(`Got ${atob(credential.largeBlob)}`);

  testRunner.completeTest();
})
