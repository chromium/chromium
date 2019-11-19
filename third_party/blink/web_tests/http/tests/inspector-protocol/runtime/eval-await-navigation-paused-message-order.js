
(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'http://first.test:8000/inspector-protocol/resources/test-page.html',
      `Tests the order in which unfinished Runtime.{enable,evaluate} calls are handled around paused navigation.`);

  // Unlike eval-avait-navigation-message-order.js, this test uses
  // the Fetch domain to pause navigation; in this way we can send additional
  // commands during the pause and observe how they're handled when navigation
  // completes.

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

  // This makes it so that we'll get a Fetch.requestPaused event after
  // issuing page.navigate below.
  await dp.Fetch.enable();

  // Navigate, this causes the error notifications to arrive - in the same order
  // in which the original requests were sent.
  const navigatePromise = page.navigate('http://second.test:8000/inspector-protocol/resources/test-page.html');

  const requestId = (await dp.Fetch.onceRequestPaused()).params.requestId;
  // The DevToolsSession is now in suspended state. Messages will be queued
  // but not executed until after the navigation is unpaused and completes.

  // Send three Runtime.enable commands with specific command ids.
  // We'll observe that these commands complete in the order in which they
  // were issued, after the navigation is unpaused.
  const saveRequestId = DevToolsAPI._requestId;
  DevToolsAPI._requestId = 100090;
  callIdsToWatch.add(100091);
  dp.Runtime.enable();
  DevToolsAPI._requestId = 100080;
  callIdsToWatch.add(100081);
  dp.Runtime.enable();
  DevToolsAPI._requestId = 100085;
  callIdsToWatch.add(100086);
  dp.Runtime.enable();

  // If we issue commands that are not idempotent, that is, commands
  // that would get terminated on cross process navigation, while a navigation
  // is paused, they'll get queued and issued after the navigation (just
  // like Runtime.enable above).
  DevToolsAPI._requestId = 100098;
  callIdsToWatch.add(100099);
  dp.Runtime.evaluate({
    expression: `console.log('Hi from request 10099!')`, awaitPromise: true
  });
  DevToolsAPI._requestId = saveRequestId;

  testRunner.log('Unpausing navigation ...');
  await dp.Fetch.continueRequest({requestId});
  await navigatePromise;

  testRunner.completeTest();
})
