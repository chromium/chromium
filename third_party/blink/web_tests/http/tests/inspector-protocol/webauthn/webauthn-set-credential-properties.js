// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command setCredentialProperties works");

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

  // Add a credential.
  const userHandle = "noah";
  const credentialId = "cred";
  await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(credentialId),
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
      isResidentCredential: true,
      userHandle: btoa(userHandle),
    }
  });

  for (const test of tests) {
    testRunner.log(`Setting ${test.prop}=${test.value}`);
    let setCredentialPropetiesOptions = {
      authenticatorId,
      credentialId: btoa(credentialId),
    };
    setCredentialPropetiesOptions[test.prop] = test.value;
    await dp.WebAuthn.setCredentialProperties(setCredentialPropetiesOptions);

    // Get an assertion.
    let assertion = await session.evaluateAsync(`getCredential({
      type: "public-key",
      id: new TextEncoder().encode("${credentialId}"),
    })`);
    testRunner.log(`Get assertion status: ${assertion.status}`);
    testRunner.log(
      `${test.flag}: ${assertion.flags[test.flag]} (expected ${test.value})`);
  }

  testRunner.completeTest();
})
