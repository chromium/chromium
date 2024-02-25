(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Check that PrivateTokenParams are included when an iframe requests a private token'`);

  await dp.Network.enable();
  await dp.Network.onRequestWillBeSent(event => {
    const privateTokenParams = event.params.request.trustTokenParams;
    if (privateTokenParams) {
      testRunner.log(`Included privateTokenParams in request: ${JSON.stringify(privateTokenParams)}`);
    } else {
      testRunner.log(`Main frame navigation not expected to contain privateTokenParams.`);
    }
  });

  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-request-trust-token.html');

  testRunner.completeTest();
})
