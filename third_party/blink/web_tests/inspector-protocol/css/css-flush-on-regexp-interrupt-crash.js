(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that interrupting V8 in ScriptRegexp context does not crash when InspectorCSSAgent flushes protocol notifications.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const evalPromise = session.evaluate(`
    const input = document.createElement('input');
    input.pattern = '(1+)0(?:0+)*\\\\1x';
    // Warm up regexp compilation before signalling ready.
    input.value = 'a';
    input.validity.patternMismatch;
    input.value = '1'.repeat(2000) + '0' + '0'.repeat(25) + '1'.repeat(2000);

    console.log('ready');

    const style = document.createElement('style');
    style.textContent = 'body { /* color: red; */ }';
    document.head.appendChild(style);
    document.body.offsetTop;

    input.validity.patternMismatch;
  `);

  dp.CSS.onStyleSheetAdded(() => testRunner.log('CSS.styleSheetAdded'));
  await dp.Runtime.onceConsoleAPICalled();
  await dp.Debugger.setBreakpointsActive({active: true});
  testRunner.log('Debugger.setBreakpointsActive completed');
  await evalPromise;
  testRunner.log('Runtime.evaluate completed');
  testRunner.completeTest();
})
