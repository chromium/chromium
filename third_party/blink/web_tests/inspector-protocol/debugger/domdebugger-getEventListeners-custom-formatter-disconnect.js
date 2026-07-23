(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <div id='target'></div>
  `,
      `Tests disconnect inside custom devtoolsFormatter called from DOMDebugger.getEventListeners.`);

  const session2 = await page.createSession();
  const dp2 = session2.protocol;

  await dp.Debugger.enable();
  await dp2.Runtime.enable();

  await session.evaluate(`
    window.devtoolsFormatters = [{
      header() {
        debugger;
        return null;
      }
    }];
    const target = document.getElementById('target');
    target.addEventListener('click', () => {}, false);
  `);

  const {result} = await dp2.Runtime.evaluate({
    expression: `document.getElementById('target')`,
    objectGroup: 'my-group'
  });
  const objectId = result.result.objectId;

  await dp2.Runtime.setCustomObjectFormatterEnabled({enabled: true});

  const completionPromise = Promise.race([
    testRunner.browserP().Target.onceDetachedFromTarget().then(
        () => new Promise(resolve => setTimeout(resolve, 500))),
    dp2.DOMDebugger.getEventListeners({objectId})
  ]);

  dp.Debugger.oncePaused().then(async () => {
    await session2.disconnect();
    dp.Debugger.disable();
  });

  // If this resolves without hitting a breakpoint, that's fine.
  // However, if it hits the breakpoint, it should not crash.
  await completionPromise;
  // Do a round-trip to renderer to assure it hasn't crashed.
  await session.evaluate('');
  testRunner.completeTest();
})
