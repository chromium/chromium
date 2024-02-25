(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests DOMSnapshot.getSnapshot method on a mobile page.');
  const DeviceEmulator = await testRunner.loadScript('../resources/device-emulator.js');
  const deviceEmulator = new DeviceEmulator(testRunner, session);
  await deviceEmulator.emulate(600, 600, 1);

  // The viewport width is 300px, half the device width.
  await session.navigate('../resources/dom-snapshot-viewport.html');

  function cleanupPaths(obj) {
    for (const key of Object.keys(obj)) {
      const value = obj[key];
      if (typeof value === 'string' && value.indexOf('/dom-snapshot/') !== -1)
        obj[key] = '<value>';
      else if (typeof value === 'object')
        cleanupPaths(value);
    }
    return obj;
  }

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': []});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(cleanupPaths(response.result), null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId']);
  testRunner.completeTest();
})
