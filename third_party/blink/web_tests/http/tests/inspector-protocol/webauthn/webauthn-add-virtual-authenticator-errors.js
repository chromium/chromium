(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the WebAuthn addVirtualAuthenticator command validates parameters");

  const disabledError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(disabledError);

  await dp.WebAuthn.enable();

  const protocolError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "nonsense",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(protocolError);

  const transportError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "nonsense",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(transportError);

  const u2fCableError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "u2f",
      transport: "cable",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(u2fCableError);

  testRunner.completeTest();
})
