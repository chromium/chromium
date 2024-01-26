(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // 1. Create a page, connect to it and use browser connection to grant it a remote debugging capability.
  const {page, session, dp} = await testRunner.startBlank('Verify that exposing devtools protocol yields a functional protocol.');
  await testRunner.browserP().Target.exposeDevToolsProtocol({targetId: page._targetId, bindingName: 'cdp'});

  // 2. To avoid implementing a protocol client in test, use target domain to validate protocol binding.
  await dp.Target.setDiscoverTargets({discover: true});

  // 3. Start target discovery and create a new target using the in-page protocol capability.
  session.evaluate(() => {
    window.messages = [];
    window.cdp.onmessage = msg => messages.push(JSON.parse(msg));
    window.cdp.send(JSON.stringify({
      id: 0,
      method: 'Target.setDiscoverTargets',
      params: {
        discover: true
      }
    }));
    window.cdp.send(JSON.stringify({
      id: 1,
      method: 'Target.createTarget',
      params: {
        url: 'about:blank'
      }
    }));
  });

  // 4. We should observe target creation.
  await dp.Target.onceTargetCreated();
  const cdpSocketMessages = await session.evaluate(() => window.messages);
  const responses = cdpSocketMessages.filter(msg => msg.hasOwnProperty('id'));
  testRunner.log('Protocol responses: ' + responses.length);
  testRunner.log('Protocol events: ' + (cdpSocketMessages.length - responses.length));
  testRunner.completeTest();
})

