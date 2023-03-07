(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that getting DOMStorage items by origin doesn't return an error response\n`);

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

  const securityOrigin = (await dp.Page.getResourceTree()).result.frameTree.frame.securityOrigin;
  errorForLog = new Error();
  const storageId = {securityOrigin, isLocalStorage: true};
  // clear storage to avoid leakage from other tests
  await dp.DOMStorage.clear({storageId});
  errorForLog = new Error();

  const items = (await dp.DOMStorage.getDOMStorageItems({storageId})).result;
  errorForLog = new Error();

  testRunner.log("Get DOM storage items");
  testRunner.log(items);

  testRunner.completeTest();
})
