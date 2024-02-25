(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
    'http://first.test:8000/inspector-protocol/resources/test-page.html',
    `Tests that not finished asynchronous Runtime.evaluate calls are terminated on navigation.`);
  const evalPromise = dp.Runtime.evaluate({
    expression: `new Promise(() => console.log('Never resolving promise created'))`,
    awaitPromise: true
  });
  testRunner.log('Evaluated promise in page');
  await page.navigate('http://second.test:8000/inspector-protocol/resources/test-page.html');
  testRunner.log('Navigated to another domain');
  const result = await evalPromise;
  testRunner.log('Evaluation response received:');
  testRunner.log(result);
  function assert(condition, message) {
    testRunner.log((!!condition ? 'PASS: ' : 'FAIL: ') + message);
  }
  assert(result.error, 'Evaluation failed after navigation');
  assert(result.error.code === -32000, 'Received server error for eval');
  assert(result.error.message === 'Inspected target navigated or closed', 'Error message mentions navigation');
  testRunner.completeTest();
})
