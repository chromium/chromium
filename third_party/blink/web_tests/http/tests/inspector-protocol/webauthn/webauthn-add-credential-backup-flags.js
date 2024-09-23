// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn credential backup flags are reflected");

  await dp.WebAuthn.enable();

  const tests = [
    { flag: "be", prop: "backupEligibility", value: true },
    { flag: "be", prop: "backupEligibility", value: false },
    { flag: "bs", prop: "backupState", value: true },
    { flag: "bs", prop: "backupState", value: false },
  ];

  // Create an authenticator.
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
      options: {
        protocol: "ctap2",
        transport: "usb",
        hasResidentKey: true,
        hasUserVerification: true,
        isUserVerified: true,
      }
    })).result.authenticatorId;

  for (const test of tests) {
    // Add a credential.
    testRunner.log("");
    testRunner.log(`Credential with ${test.prop}=${test.value}`);
    const userHandle = "noah";
    const credentialId = "cred";
    let credentialOptions = {
      authenticatorId,
      credential: {
        credentialId: btoa(credentialId),
        rpId: "devtools.test",
        privateKey: await session.evaluateAsync("generateBase64Key()"),
        signCount: 0,
        isResidentCredential: true,
        userHandle: btoa(userHandle),
      }
    };
    credentialOptions.credential[test.prop] = test.value;
    await dp.WebAuthn.addCredential(credentialOptions);

    // Get an assertion.
    let assertion = await session.evaluateAsync(`getCredential({
      type: "public-key",
      id: new TextEncoder().encode("${credentialId}"),
    })`);
    testRunner.log(`Get assertion status: ${assertion.status}`);
    testRunner.log(
      `${test.flag}: ${assertion.flags[test.flag]} (expected ${test.value})`);

    // Verify that getting the credential through devtools reflects the value.
    let credential = (await dp.WebAuthn.getCredential({
      authenticatorId, credentialId: btoa(credentialId)
    })).result.credential;
    testRunner.log("Get credential through devtools");
    testRunner.log(
      `${test.prop}: ${credential[test.prop]} (expected ${test.value})`);

    // Clean up the credential.
    await dp.WebAuthn.removeCredential({
      authenticatorId, credentialId: btoa(credentialId)
    });
  }

  testRunner.completeTest();
})
