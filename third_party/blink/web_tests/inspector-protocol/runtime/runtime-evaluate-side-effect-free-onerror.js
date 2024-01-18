(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that side-effect free Runtime.evaluate does not trigger window.onerror.`);

  session.evaluate(`
    window.onerrorCalled = false;
    window.onerror = event => {
      window.onerrorCalled = true;
      event.preventDefault();
    }
  `);

  await dp.Runtime.evaluate({
    expression: 'nonexistent',
    replMode: true,
    throwOnSideEffect: true,
  });

  const onerrorCalled =
      await dp.Runtime.evaluate({expression: 'window.onerrorCalled'});

  testRunner.log(
      onerrorCalled ?
          'Side-effect free Runtime.evaluate error didn\'t trigger window.onerror' :
          '[FAIL] Side-effect free Runtime.evaluate triggered window.onerror');
  testRunner.completeTest();
})
