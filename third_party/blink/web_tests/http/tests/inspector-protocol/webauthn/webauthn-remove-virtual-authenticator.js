(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the WebAuthn removeVirtualAuthenticator command works");

  const disabledError = await dp.WebAuthn.removeVirtualAuthenticator({
    authenticatorId: "id",
  });
  testRunner.log(disabledError);

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  const response = await dp.WebAuthn.removeVirtualAuthenticator({
    authenticatorId,
  });
  testRunner.log(response);

  const notFoundError = await dp.WebAuthn.removeVirtualAuthenticator({
    authenticatorId: "id",
  });
  testRunner.log(notFoundError);

  testRunner.completeTest();
})
