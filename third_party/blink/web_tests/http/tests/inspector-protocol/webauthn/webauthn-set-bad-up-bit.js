(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html',
      'Check that the WebAuthn command setBadUVBit works');

  // Create an authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
                            options: {
                              protocol: 'ctap2',
                              transport: 'usb',
                              hasResidentKey: true,
                              hasUserVerification: false,
                            },
                          })).result.authenticatorId;

  // Register a non-resident credential.
  const nonResidentCredentialId = 'cred-1';
  testRunner.log(await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(nonResidentCredentialId),
      rpId: 'devtools.test',
      privateKey: await session.evaluateAsync('generateBase64Key()'),
      signCount: 0,
      isResidentCredential: false,
    }
  }));

  // Get UP bit from flags from authenticator data in response.
  testRunner.log(await session.evaluateAsync(`getFlags(0)`));

  // Set UP bit in authenticator response to always be 0.
  testRunner.log(await dp.WebAuthn.setResponseOverrideBits(
      {authenticatorId, isBadUP: true}));

  // Get UP bit from flags from authenticator data in response.
  testRunner.log(await session.evaluateAsync(`getFlags(0)`));
  testRunner.completeTest();
})
