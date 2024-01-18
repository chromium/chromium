(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html',
      'Check that the WebAuthn command setAutomaticPresenceSimulation works');

  await dp.WebAuthn.enable();
  const authenticatorId1 = (await dp.WebAuthn.addVirtualAuthenticator({
                             options: {
                               protocol: 'ctap2',
                               transport: 'usb',
                               hasResidentKey: true,
                               hasUserVerification: true,
                               isUserVerified: true,
                               automaticPresenceSimulation: true,
                             },
                           })).result.authenticatorId;

  const authenticatorId2 = (await dp.WebAuthn.addVirtualAuthenticator({
                             options: {
                               protocol: 'ctap2',
                               transport: 'usb',
                               hasResidentKey: true,
                               hasUserVerification: true,
                               isUserVerified: true,
                               automaticPresenceSimulation: true,
                             },
                           })).result.authenticatorId;

  // Set authenticator 1 APS to false, create credential.
  testRunner.log(await dp.WebAuthn.setAutomaticPresenceSimulation(
      {authenticatorId: authenticatorId1, enabled: false}));
  testRunner.log((await session.evaluateAsync('registerCredential()')).status);

  // Check that authenticator 1 didn't register first credential and that
  // authenticator 2 did.
  testRunner.log((await dp.WebAuthn.getCredentials({
                   authenticatorId: authenticatorId1
                 })).result.credentials.length);  // Should be 0.
  testRunner.log((await dp.WebAuthn.getCredentials({
                   authenticatorId: authenticatorId2
                 })).result.credentials.length);  // Should be 1.

  // Set authenticator 1 APS to true, set authenticator 2 APS to false, create
  // credential.
  testRunner.log(await dp.WebAuthn.setAutomaticPresenceSimulation(
      {authenticatorId: authenticatorId1, enabled: true}));
  testRunner.log(await dp.WebAuthn.setAutomaticPresenceSimulation(
      {authenticatorId: authenticatorId2, enabled: false}));

  testRunner.log((await session.evaluateAsync('registerCredential()')).status);

  // Check that authenticator 1 did register second credential and that
  // authenticator 2 didn't.
  testRunner.log((await dp.WebAuthn.getCredentials({
                   authenticatorId: authenticatorId1
                 })).result.credentials.length);  // Should be 1.
  testRunner.log((await dp.WebAuthn.getCredentials({
                   authenticatorId: authenticatorId2
                 })).result.credentials.length);  // Should be 1.

  testRunner.completeTest();
})
