(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that there is no ResourceTiming object in responseReceived for file urls.`);

  await dp.Network.enable();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  session.navigate('./resources/simple.html');
  const responseReceived = await responseReceivedPromise;
  testRunner.log('response.timing present: ' + !!responseReceived.params.response.timing);
  testRunner.completeTest();
})
