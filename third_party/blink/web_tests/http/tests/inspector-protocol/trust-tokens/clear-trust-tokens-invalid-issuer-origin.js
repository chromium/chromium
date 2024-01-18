(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
    `Check that calling Storage.clearTrustTokens with an invalid issuerOrigin produces an error`);

  testRunner.expectedSuccess(
      'Storage.clearTrustTokens with valid origin',
      await dp.Storage.clearTrustTokens({issuerOrigin: 'https://example.org'}));

  testRunner.expectedError(
      'Storage.clearTrustTokens with invalid origin',
      await dp.Storage.clearTrustTokens({issuerOrigin: 'ws://example.org'}));

  testRunner.completeTest();
})
