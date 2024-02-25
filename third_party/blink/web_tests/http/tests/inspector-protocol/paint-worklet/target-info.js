(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests target info reported by paint worklet.`);

  await dp.Network.enable()
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  session.evaluateAsync(`CSS.paintWorklet.addModule('${testRunner.url('resources/blank-worklet.js')}')`);
  const cssWorklet = (await dp.Target.onceAttachedToTarget()).params;
  testRunner.log(cssWorklet);

  testRunner.completeTest();
})
