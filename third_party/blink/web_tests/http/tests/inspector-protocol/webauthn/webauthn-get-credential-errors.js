(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the WebAuthn command getCredential validates parameters");

  // Try without enabling the WebAuthn environment.
  let credentialId = btoa("roses are red");
  testRunner.log(await dp.WebAuthn.getCredential({
    authenticatorId: "nonsense",
    credentialId,
  }));

  // Try for an authenticator that does not exist.
  await dp.WebAuthn.enable();
  testRunner.log(await dp.WebAuthn.getCredential({
    authenticatorId: "nonsense",
    credentialId,
  }));

  // Try for a credentialId that does not exist.
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
    },
  })).result.authenticatorId;
  testRunner.log(await dp.WebAuthn.getCredential({
    authenticatorId,
    credentialId,
  }));

  testRunner.completeTest();
})
