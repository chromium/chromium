// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command setCredentialProperties validates parameters");

  await dp.WebAuthn.enable();

  testRunner.log("Unknown credential and authenticator:");
  testRunner.log(await dp.WebAuthn.setCredentialProperties({
    authenticatorId: btoa("unknown authenticator"),
    credentialId: btoa("unknown credential"),
  }));

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
  testRunner.log("Unknown credential with a valid authenticator:");
  testRunner.log(await dp.WebAuthn.setCredentialProperties({
    authenticatorId,
    credentialId: btoa("unknown credential"),
  }));

  // Add a credential.
  const userHandle = btoa("noah");
  const credentialId = btoa("cred");
  await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId,
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
      isResidentCredential: true,
      userHandle,
    }
  });
  testRunner.log("Valid credential and authenticator:");
  testRunner.log(await dp.WebAuthn.setCredentialProperties({
    authenticatorId, credentialId,
  }));

  testRunner.completeTest();
})
