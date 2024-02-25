(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn virtual authenticator supports all algorithms");

  // Create a CTAP2 authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  let evaluateAlgorithm = async algo => {
    let result = await session.evaluateAsync(`registerCredential({
      pubKeyCredParams: [{type: "public-key", alg: ${algo}}]
    })`);
    testRunner.log(`Algorithm: ${algo}`);
    testRunner.log(`Result: ${result.status === 'OK' ? 'OK' : 'Error'}`);
  };

  const supportedAlgorithms = [-7, -8, -257];
  testRunner.log("Supported algorithms:");
  for (const algo of supportedAlgorithms) {
    await evaluateAlgorithm(algo);
  }
  testRunner.log("\nInvalid algorithm:");
  const invalidAlgorithmForTesting = 146919568;
  await evaluateAlgorithm(invalidAlgorithmForTesting);

  testRunner.completeTest();
})
