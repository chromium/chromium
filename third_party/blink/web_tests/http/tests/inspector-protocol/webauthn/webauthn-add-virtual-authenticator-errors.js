(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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

  const ctapVersionError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      ctap2Version: "nonsense",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(ctapVersionError);

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
      transport: "hybrid",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  });
  testRunner.log(u2fCableError);

  const largeBlobRequiresRKError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      ctap2Version: "ctap2_0",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
      hasLargeBlob: true
    },
  });
  testRunner.log(largeBlobRequiresRKError);

  const largeBlobRequiresCtapError = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "u2f",
      ctap2Version: "ctap2_1",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
      hasLargeBlob: true
    },
  });
  testRunner.log(largeBlobRequiresCtapError);

  const largeBlobRequiresCtap2_1Error = await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      ctap2Version: "ctap2_0",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
      hasLargeBlob: true
    },
  });
  testRunner.log(largeBlobRequiresCtap2_1Error);

  testRunner.completeTest();
})
