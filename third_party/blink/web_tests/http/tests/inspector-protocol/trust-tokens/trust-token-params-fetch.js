(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Check that PrivateTokenParams are included in the basic Private Token operations on 'fetch'`);

  const clearPrivateTokenState = async () => {
    await session.evaluateAsync(`await new Promise(res => window.testRunner.clearTrustTokenState(res));`);
  };

  const issuanceRequest = `
    fetch('https://trusttoken.test', {
      privateToken: {
        version: 1,
        operation: 'token-request'
      }
    });
  `;

  const redemptionRequest = `
    fetch('https://trusttoken.test', {
      privateToken: {
        version: 1,
        operation: 'token-redemption'
      }
    });
  `;

  const signingRequest = `
    fetch('https://destination.test', {
      privateToken: {
        version: 1,
        operation: 'send-redemption-record',
        issuers: ['https://issuer.test']
      }
    });
  `;

  // Note that the requests are failing, as the provided URLs are neither valid
  // issuers, nor redeemers. This test only cares about the parameters included
  // in the requests.

  await dp.Network.enable();
  await dp.Network.onRequestWillBeSent(event => {
    const privateTokenParams = event.params.request.trustTokenParams;
    testRunner.log(`Included privateTokenParams in request: ${JSON.stringify(privateTokenParams)}`);
  });

  for (const request of [issuanceRequest, redemptionRequest, signingRequest]) {
    testRunner.log(`Sending request: ${request}`);
    await session.evaluateAsync(request);
    await clearPrivateTokenState();
  }

  testRunner.completeTest();
})
