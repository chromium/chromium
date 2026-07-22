// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that CMTG keys can be managed via DevTools Protocol");

  await dp.WebAuthn.enable();

  // Create an authenticator without CMTG support.
  const noCmtgAuthenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      isUserVerified: true,
      hasCmtgKey: false,
    }
  })).result.authenticatorId;

  // Register a credential requesting CMTG key.
  testRunner.log("Registering credential with CMTG on an authenticator without support...");
  const noCmtgRegisterResult = await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      requireResidentKey: true,
      userVerification: "required",
    },
    extensions: {
      cmtgKey: true,
    }
  })`);
  testRunner.log(`Register status: ${noCmtgRegisterResult.status}`);
  testRunner.log(`CMTG key: ${noCmtgRegisterResult.cmtgKey ? "present" : "missing"}`);
  await dp.WebAuthn.removeVirtualAuthenticator({
    authenticatorId: noCmtgAuthenticatorId,
  });

  // Create an authenticator with CMTG support.
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: true,
      isUserVerified: true,
      hasCmtgKey: true,
    }
  })).result.authenticatorId;

  // Register a credential requesting CMTG key.
  testRunner.log("Registering credential with CMTG on an authenticator with support...");
  const registerResult = await session.evaluateAsync(`registerCredential({
    authenticatorSelection: {
      requireResidentKey: true,
      userVerification: "required",
    },
    extensions: {
      cmtgKey: true,
    }
  })`);
  testRunner.log(`Register status: ${registerResult.status}`);
  const keyA = registerResult.cmtgKey?.cmtgKey;
  const sigA = registerResult.cmtgKey?.signature;
  testRunner.log(`Generated CMTG public key A: ${keyA ? "present" : "missing"}`);
  testRunner.log(`Generated CMTG signature A: ${sigA ? "present" : "missing"}`);

  // Verify credential state via getCredentials.
  testRunner.log("Retrieving credentials...");
  let credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(`Number of credentials: ${credentials.length}`);
  const cred = credentials[0];
  testRunner.log(`Credential has cmtgKeys: ${cred.cmtgKeys !== undefined}`);
  testRunner.log(`cmtgKeys count: ${cred.cmtgKeys?.length}`);
  testRunner.log(`activeCmtgKeyIndex: ${cred.activeCmtgKeyIndex}`);
  testRunner.log(`generateCmtgKeyOnNextOperation: ${cred.generateCmtgKeyOnNextOperation}`);

  const credentialId = cred.credentialId;

  // Get assertion and verify it uses key A and returns a signature.
  testRunner.log("Getting assertion...");
  let assertResult = await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: base64ToArrayBuffer("${credentialId}"),
  }, {
    extensions: {
      cmtgKey: true,
    }
  })`);
  testRunner.log(`Assertion status: ${assertResult.status}`);
  testRunner.log(`Assertion key matches key A: ${assertResult.cmtgKey?.cmtgKey === keyA}`);
  testRunner.log(`Assertion signature present: ${assertResult.cmtgKey?.signature ? "yes" : "no"}`);

  // Inject a credential with pre-configured CMTG keys.
  testRunner.log('Injecting credential with CMTG keys...');
  const injectedKeyA = await session.evaluateAsync('generateBase64Key()');
  const injectedKeyB = await session.evaluateAsync('generateBase64Key()');
  const injectedCredId = btoa('injected-cmtg-cred');
  await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: injectedCredId,
      rpId: 'devtools.test',
      privateKey: await session.evaluateAsync('generateBase64Key()'),
      signCount: 0,
      isResidentCredential: true,
      userHandle: btoa('nina'),
      cmtgKeys: [injectedKeyA, injectedKeyB],
      activeCmtgKeyIndex: 1,
    }
  });

  // Verify injected credential state.
  testRunner.log('Retrieving credentials after injection...');
  credentials =
      (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  const injectedCred = credentials.find(c => c.credentialId === injectedCredId);
  testRunner.log(`Injected credential found: ${injectedCred !== undefined}`);
  testRunner.log(
      `Injected credential cmtgKeys count: ${injectedCred?.cmtgKeys?.length}`);
  testRunner.log(`Injected credential activeCmtgKeyIndex: ${
      injectedCred?.activeCmtgKeyIndex}`);
  testRunner.log(`Injected key 0 matches: ${
      injectedCred?.cmtgKeys?.[0] === injectedKeyA}`);
  testRunner.log(`Injected key 1 matches: ${
      injectedCred?.cmtgKeys?.[1] === injectedKeyB}`);

  testRunner.completeTest();
})
