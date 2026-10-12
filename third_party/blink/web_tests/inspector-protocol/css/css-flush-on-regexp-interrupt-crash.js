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
    // Calibrate input size (and warm up regexp compilation) before signalling
    // ready, as MSan runs V8 JIT code under the ARM64 simulator.
    for (let n = 1; ; ++n) {
      const start = performance.now();
      input.value = '1'.repeat(1 << n) + '0' + '0'.repeat(n) + '1'.repeat(1 << n);
      input.validity.patternMismatch;
      if (performance.now() - start > 100) {
        ++n;
        input.value =
            '1'.repeat(1 << n) + '0' + '0'.repeat(n + 1) + '1'.repeat(1 << n);
        break;
      }
    }

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
