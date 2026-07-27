// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html',
      'Check that the WebAuthn signature counter can be set with devtools');

  await dp.WebAuthn.enable();

  // Create an authenticator.
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
                            options: {
                              protocol: 'ctap2',
                              transport: 'usb',
                              hasResidentKey: true,
                              hasUserVerification: true,
                              isUserVerified: true,
                            }
                          })).result.authenticatorId;

  // Add credential with no sign count (signCount = -1).
  const credentialId = 'cred-no-counter';
  await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(credentialId),
      rpId: 'devtools.test',
      privateKey: await session.evaluateAsync('generateBase64Key()'),
      signCount: -1,  // No counter.
      isResidentCredential: true,
      userHandle: btoa('user'),
    }
  });

  // Verify stored credential has signCount = -1.
  let credentials =
      (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(`Stored credential signCount: ${credentials[0].signCount}`);

  // Get assertion.
  let assertion = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${credentialId}"),
  })`);
  testRunner.log(`Assertion 1 signCount: ${assertion.signCount}`);

  // Get another assertion to verify it doesn't increment.
  assertion = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${credentialId}"),
  })`);
  testRunner.log(`Assertion 2 signCount: ${assertion.signCount}`);

  // Set signature counter to 10.
  await dp.WebAuthn.setCredentialProperties({
    authenticatorId,
    credentialId: btoa(credentialId),
    signCount: 10,
  });

  // Verify stored credential has signCount = 10.
  credentials =
      (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(`Stored credential signCount after update to 10: ${
      credentials[0].signCount}`);

  // Get assertion, should increment to 11.
  assertion = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${credentialId}"),
  })`);
  testRunner.log(
      `Assertion after update (expected 11): ${assertion.signCount}`);

  // Set signature counter to -1 (remove it).
  await dp.WebAuthn.setCredentialProperties({
    authenticatorId,
    credentialId: btoa(credentialId),
    signCount: -1,
  });

  // Get assertion, should return 0.
  assertion = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${credentialId}"),
  })`);
  testRunner.log(
      `Assertion after removing counter (expected 0): ${assertion.signCount}`);

  testRunner.completeTest();
})
