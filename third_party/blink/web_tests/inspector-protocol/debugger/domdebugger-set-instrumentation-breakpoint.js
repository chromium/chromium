(async function testDomDebuggerInstrumentationBreakpoint(testRunner) {
  const targetPage = 'resources/debugger-basic-page.html';
  const { dp } = await testRunner.startBlank('Tests the DOMDebugger.setInstrumentationBreakpoint API.');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();
  const targetPageUrl = testRunner.url(targetPage);

  testRunner.runTestSuite([
    async function testScriptFirstStatement() {
      await dp.DOMDebugger.setInstrumentationBreakpoint({ eventName: 'scriptFirstStatement' });
      dp.Page.navigate({ url: targetPageUrl });
      const pauseMessage = await dp.Debugger.oncePaused();
      testRunner.log(pauseMessage.params.data);
    },
    async function testRemoveScriptFirstStatement() {
      await dp.DOMDebugger.removeInstrumentationBreakpoint({ eventName: 'scriptFirstStatement' });
      dp.Page.navigate({ url: targetPageUrl });
      await dp.Runtime.evaluate({ expression: 'test();' }); // Will stall if bp still active
    },
    async function testInvalidEventName() {
      const setResponse = await dp.DOMDebugger.setInstrumentationBreakpoint({ eventName: 'badEventName' });
      testRunner.log(setResponse);
    }
  ]);
})