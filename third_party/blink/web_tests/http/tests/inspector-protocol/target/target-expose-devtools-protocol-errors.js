(async function(testRunner) {
  // 1. Create a page, connect to it and use browser connection to grant it a remote debugging capability.
  const {page, session, dp} = await testRunner.startBlank(
      'Verify that errors in the protocol handlers are dispatched in the page.');
  await testRunner.browserP().Target.exposeDevToolsProtocol({targetId: page._targetId, bindingName: 'cdp'});

  // 2. To avoid implementing a protocol client in test, use target domain to validate protocol binding.
  await dp.Target.setDiscoverTargets({discover: true});

  dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(result => {
    testRunner.log(result.params.args[0].description);
    testRunner.completeTest();
  });

  session.evaluate(() => {
    // Redirect unhandled errors into console.
    window.onerror = msg => console.log('Unhandled error: ' + msg);
    // Inject unhandled error.
    window.cdp.onmessage = msg => a = c;
    window.cdp.send(JSON.stringify({
      id: 0,
      method: 'Target.setDiscoverTargets',
      params: {
        discover: true
      }
    }));
  });
})

