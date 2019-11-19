(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'http://first.test:8000/inspector-protocol/resources/test-page.html',
      `Tests the order in which not finished asynchronous Runtime.evaluate calls are temintated on navigation.`);

  // Runtime.enable so we can capture console.log below. This command completes.
  await dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(e => { testRunner.log(e.params.args[0].value); });

  const callIdsToWatch = new Set();

  // Creates a promise which doesn't resolve, so therefore, we won't get a
  // response - only once we've navigated below, we'll get an error response
  // due to command termination. Request ID will be call_id.
  async function createNonResolvingPromise(call_id) {
    const saveRequestId = DevToolsAPI._requestId;
    DevToolsAPI._requestId = call_id - 1;
    dp.Runtime.evaluate({
      expression: `new Promise(() => console.log('promise created (` + call_id + `)'))`,
      awaitPromise: true
    });
    DevToolsAPI._requestId = saveRequestId;
    await dp.Runtime.onceConsoleAPICalled();  // Be sure to capture the log.
    callIdsToWatch.add(call_id);
  }

  // Override the dispatch routine so we can intercept and log the responses in
  // the order in which they arrive.
  const originalDispatch = DevToolsAPI.dispatchMessage;
  DevToolsAPI.dispatchMessage = function(message) {
    var obj = JSON.parse(message);
    if (callIdsToWatch.has(obj.id)) {
      testRunner.log(obj, 'receiving result ' + obj.id + ':\n', ['sessionId']);
    }
    originalDispatch(message);
  }

  // Now create three promises with out-of-order call ids.
  await createNonResolvingPromise(100021);
  await createNonResolvingPromise(100013);
  await createNonResolvingPromise(100017);

  // Navigate, this causes the error notifications to arrive - in the same order
  // in which the original requests were sent.
  await page.navigate('http://second.test:8000/inspector-protocol/resources/test-page.html');

  testRunner.completeTest();
})
