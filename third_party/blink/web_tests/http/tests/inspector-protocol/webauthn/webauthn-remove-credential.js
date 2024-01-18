(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command removeCredential works");

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // Register two credentials.
  const result1 = await session.evaluateAsync("registerCredential()");
  testRunner.log(result1.status);
  let credential1Id =
      await session.evaluate(`base64urlToBase64("${result1.credential.id}")`);

  const result2 = await session.evaluateAsync("registerCredential()");
  testRunner.log(result2.status);
  let credential2Id =
      await session.evaluate(`base64urlToBase64("${result2.credential.id}")`);

  let credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(credentials.length);

  // Remove the first credential.
  testRunner.log(await dp.WebAuthn.removeCredential({
    authenticatorId,
    credentialId: credential1Id
  }));

  // Only the second credential should remain.
  credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(credentials.length);
  testRunner.log(
      credentials[0].credentialId == credential2Id ? "IDs match" : "IDs do not match");

  testRunner.completeTest();
})
