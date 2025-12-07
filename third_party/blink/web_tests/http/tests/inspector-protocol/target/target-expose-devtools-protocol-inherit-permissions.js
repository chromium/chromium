(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank('Verify that exposing devtools protocol can inherit permissions.');

  async function testUnsafeCommandAvailability(inheritPermissions, unsafeOperationsAllowed) {
    testRunner.setAllowUnsafeOperations(unsafeOperationsAllowed);

    const page = await testRunner.createPage({
      url: 'about:blank',
    });

    await testRunner.browserP().Target.exposeDevToolsProtocol({
      targetId: page.targetId(),
      bindingName: 'cdp',
      inheritPermissions,
    });

    const session = await page.createSession();

    const targetPage = await testRunner.createPage({
      url: 'about:blank',
    });

    const result = await session.evaluateAsync(async (targetId) => {
      function send(command) {
        return new Promise(resolve => {
          cdp.onmessage = (event) => {
            const parsed = JSON.parse(event);
            if (parsed.id === command.id) {
              resolve(parsed)
            }
          };
          cdp.send(JSON.stringify(command));
        });
      }
      let result = await send({
        id: 1,
        method: 'Target.attachToTarget',
        params: { targetId, flatten: true }
      });
      const sessionId = result.result.sessionId;
      result = await send({
        id: 2,
        sessionId,
        method: 'Page.addCompilationCache',
        params: {
          url: 'http://www.example.com/hello.js',
          data: 'Tm90aGluZyB0byBzZWUgaGVyZSE='
        }
      });
      return 'result' in result;
    }, targetPage.targetId());

    testRunner.log(`Inherit permissions: ${inheritPermissions}, command available: ${result}`);
  }

  for (const inherit of [true, false]) {
    for (const unsafeOperationsAllowed of [true, false]) {
      await testUnsafeCommandAvailability(inherit, unsafeOperationsAllowed);
    }
  }

  testRunner.completeTest();
})
