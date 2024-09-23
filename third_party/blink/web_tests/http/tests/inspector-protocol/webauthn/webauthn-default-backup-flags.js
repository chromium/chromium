// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn default backup flags are reflected");

  await dp.WebAuthn.enable();

  const tests = [
    { option: "defaultBackupEligibility", flag: "be", prop: "backupEligibility", value: true },
    { option: "defaultBackupEligibility", flag: "be", prop: "backupEligibility", value: false },
    { option: "defaultBackupState", flag: "bs", prop: "backupState", value: true },
    { option: "defaultBackupState", flag: "bs", prop: "backupState", value: false },
  ];

  for (const test of tests) {
    // Create an authenticator with the default set by `test`.
    testRunner.log("");
    testRunner.log(`Authenticator with ${test.option}=${test.value}`);
    let authenticatorOptions = {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      isUserVerified: true,
    };
    authenticatorOptions[test.option] = test.value;
    const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator(
      {options: authenticatorOptions})).result.authenticatorId;

    // Register a credential through the webauthn API.
    const result = (await session.evaluateAsync(`registerCredential({
      authenticatorSelection: {
        requireResidentKey: true,
      },
    })`));
    testRunner.log(`Create credential status: ${result.status}`);
    testRunner.log(
      `${test.flag}: ${result.flags[test.flag]} (expected ${test.value})`);

    // Get an assertion from the credential created through webauthn.
    const credentialId = await session.evaluate(
      `base64urlToBase64("${result.credential.id}")`);
    let assertion = await session.evaluateAsync(`getCredential({
      type: "public-key",
      id: base64ToArrayBuffer("${credentialId}"),
    })`);
    testRunner.log(`Get assertion status: ${assertion.status}`);
    testRunner.log(
      `${test.flag}: ${assertion.flags[test.flag]} (expected ${test.value})`);

    // Verify that getting the credential through devtools reflects the value.
    let credential =
        (await dp.WebAuthn.getCredential({authenticatorId, credentialId})).result.credential;
    testRunner.log("Get credential through devtools");
    testRunner.log(
      `${test.prop}: ${credential[test.prop]} (expected ${test.value})`);

    // Verify that the default also applies to credentials created through the
    // devtools protocol.
    testRunner.log("Replacing credential by one added through devtools");
    await dp.WebAuthn.removeCredential({
      authenticatorId,
      credentialId
    });
    await dp.WebAuthn.addCredential({
      authenticatorId,
      credential: {
        credentialId: credentialId,
        rpId: "devtools.test",
        privateKey: await session.evaluateAsync("generateBase64Key()"),
        signCount: 0,
        isResidentCredential: true,
        userHandle: credentialId,
      }
    });

    // Get an assertion from the credential created through devtools.
    assertion = await session.evaluateAsync(`getCredential({
      type: "public-key",
      id: base64ToArrayBuffer("${credentialId}"),
    })`);
    testRunner.log(`Get credential status: ${assertion.status}`);
    testRunner.log(
      `${test.flag}: ${assertion.flags[test.flag]} (expected ${test.value})`);

    await dp.WebAuthn.removeVirtualAuthenticator({authenticatorId});
  }

  testRunner.completeTest();
})
