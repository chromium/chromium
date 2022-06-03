(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Check that TrustTokenParams are included when an iframe requests a trust token'`);

  await dp.Network.enable();
  await dp.Network.onRequestWillBeSent(event => {
    const trustTokenParams = event.params.request.trustTokenParams;
    if (trustTokenParams) {
      testRunner.log(`Included trustTokenParams in request: ${JSON.stringify(trustTokenParams)}`);
    } else {
      testRunner.log(`Main frame navigation not expected to contain trustTokenParams.`);
    }
  });

  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-request-trust-token.html');

  testRunner.completeTest();
})
