(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that WebAuthn hmac-secret and hmac-secret-mc extensions work");

  const firstSalt = new Array(32).fill(0);
  const secondSalt = new Array(32).fill(1);

  // Serialize salts as Uint8Array expressions for use in evaluateAsync.
  function serializePRF(salts) {
    let s = `prf:{eval:{first: new Uint8Array(${JSON.stringify(salts.first)})`;
    if ('second' in salts) {
      s += `, second: new Uint8Array(${JSON.stringify(salts.second)})`;
    }
    s += '}}';
    return s;
  }

  // Helper to register a credential with PRF and log the results.
  async function createCredential(salts) {
    const label = `${('second' in salts) ? "with" : "without"} second`;
    const result = await session.evaluateAsync(`registerCredential({
      extensions: {${serializePRF(salts)}},
      authenticatorSelection: {
        userVerification: "required",
      },
    })`);
    testRunner.log(`Create credential result (${label}): ${result.status}`);
    return result;
  }

  // Helper to get an assertion with PRF and log the results.
  async function getCredential(credentialId, salts) {
    const label = `${('second' in salts) ? "with" : "without"} second`;
    const result = await session.evaluateAsync(`getCredential({
      type: "public-key",
      id: base64ToArrayBuffer(base64urlToBase64("${credentialId}")),
      transports: ["usb"],
    }, {
      userVerification: "required",
      extensions: {${serializePRF(salts)}},
    })`);
    testRunner.log(`Get credential result (${label}): ${result.status}`);
    return result;
  }

  // Helper to reset and create a virtual authenticator with given options.
  async function addVirtualAuthenticator(options) {
    await dp.WebAuthn.disable();
    await dp.WebAuthn.enable();
    await dp.WebAuthn.addVirtualAuthenticator({
      options: Object.assign({
        protocol: "ctap2",
        transport: "usb",
        hasUserVerification: true,
        isUserVerified: true,
      }, options),
    });
  }

  // No hmac-secret support: PRF should not be enabled.
  testRunner.log("# no hmac-secret");
  await addVirtualAuthenticator({ctap2Version: "ctap2_1"});

  let createResult = await createCredential({first: firstSalt});
  testRunner.log(`prf.enabled: ${createResult.prf.enabled}`);
  testRunner.log(`prf.results exists: ${"results" in createResult.prf}`);

  // hmac-secret only (CTAP 2.1): PRF enabled but no results from create().
  testRunner.log("# hmac-secret");
  await addVirtualAuthenticator({ctap2Version: "ctap2_1", hasHmacSecret: true});

  let salts = {first: firstSalt};
  createResult = await createCredential(salts);
  testRunner.log(`prf.enabled: ${createResult.prf.enabled}`);
  testRunner.log(`prf.results exists: ${"results" in createResult.prf}`);
  let getResult = await getCredential(createResult.credential.id, salts);
  testRunner.log(`prf.results.first exists: ${"first" in getResult.prf.results}`);
  testRunner.log(`prf.results.second exists: ${"second" in getResult.prf.results}`);

  salts = {first: firstSalt, second: secondSalt};
  createResult = await createCredential(salts);
  testRunner.log(`prf.enabled: ${createResult.prf.enabled}`);
  testRunner.log(`prf.results exists: ${"results" in createResult.prf}`);
  getResult = await getCredential(createResult.credential.id, salts);
  testRunner.log(`prf.results.first exists: ${"first" in getResult.prf.results}`);
  testRunner.log(`prf.results.second exists: ${"second" in getResult.prf.results}`);

  // hmac-secret-mc (CTAP 2.2): PRF results available from create().
  testRunner.log("# hmac-secret-mc");
  await addVirtualAuthenticator({
    ctap2Version: "ctap2_2",
    hasHmacSecret: true,
    hasHmacSecretMc: true,
  });

  salts = {first: firstSalt};
  createResult = await createCredential(salts);
  testRunner.log(`prf.enabled: ${createResult.prf.enabled}`);
  testRunner.log(`prf.results.first exists: ${"first" in createResult.prf.results}`);
  testRunner.log(`prf.results.second exists: ${"second" in createResult.prf.results}`);
  getResult = await getCredential(createResult.credential.id, salts);
  testRunner.log(`prf.results.first matches: ${createResult.prf.results.first === getResult.prf.results.first}`);
  testRunner.log(`prf.results.second exists: ${"second" in getResult.prf.results}`);

  salts = {first: firstSalt, second: secondSalt};
  createResult = await createCredential(salts);
  testRunner.log(`prf.results.first exists: ${"first" in createResult.prf.results}`);
  testRunner.log(`prf.results.second exists: ${"second" in createResult.prf.results}`);
  getResult = await getCredential(createResult.credential.id, salts);
  testRunner.log(`prf.results.first matches: ${createResult.prf.results.first === getResult.prf.results.first}`);
  testRunner.log(`prf.results.second matches: ${createResult.prf.results.second === getResult.prf.results.second}`);

  testRunner.completeTest();
})
