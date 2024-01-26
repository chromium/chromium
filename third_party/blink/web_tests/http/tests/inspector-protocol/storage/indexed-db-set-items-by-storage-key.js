(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests that setting IndexedDB data by storage key works differently to doing so by origin\n`);

  const protocolMessages = [];
  const originalDispatchMessage = DevToolsAPI.dispatchMessage;
  const originalSendCommand = DevToolsAPI._sendCommand;
  DevToolsAPI.dispatchMessage = (message) => {
    protocolMessages.push(message);
    originalDispatchMessage(message);
  };
  DevToolsAPI._sendCommand = (sessionId, method, params) => {
    protocolMessages.push({sessionId, method, params});
    return originalSendCommand(sessionId, method, params);
  }
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.log(protocolMessages);
    testRunner.die('Took longer than 25s', errorForLog);
  }, 25000);

  await dp.DOMStorage.enable();
  errorForLog = new Error();
  await dp.Page.enable();
  errorForLog = new Error();

  testRunner.log("Set storage item on an iframe of page with one origin:");
  await page.navigate('http://devtools-origin1.test:8000/inspector-protocol/resources/page-with-frame-indexed-db.html');
  errorForLog = new Error();
  await session.evaluateAsync(`window.onMessagePromise`);
  errorForLog = new Error();
  testRunner.log("item set\n");

  testRunner.log("Read item from an iframe of page with other origin: ");
  await page.navigate('http://devtools-origin2.test:8000/inspector-protocol/resources/page-with-frame-indexed-db.html');
  errorForLog = new Error();
  testRunner.log(await session.evaluateAsync(`window.onMessagePromise`));
  errorForLog = new Error();

  testRunner.completeTest();
})

