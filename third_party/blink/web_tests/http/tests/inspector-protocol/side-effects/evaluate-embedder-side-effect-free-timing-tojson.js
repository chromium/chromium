(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Performance and PerformanceEntry toJSON() serialization is side-effect-free.');

  await session.evaluate(`
    performance.mark('test-mark');
  `);

  // Direct Performance, PerformanceTiming, and PerformanceNavigation toJSON.
  await checkHasNoSideEffect(`performance.toJSON()`);
  await checkHasNoSideEffect(`performance.timing.toJSON()`);
  await checkHasNoSideEffect(`performance.navigation.toJSON()`);
  await checkHasNoSideEffect(`JSON.stringify(performance)`);

  // Full buffered timeline serialization.
  await checkHasNoSideEffect(`JSON.stringify(performance.getEntries())`);
  await checkHasNoSideEffect(
      `performance.getEntriesByType('navigation')[0].toJSON()`);

  // Base PerformanceEntry#toJSON via PerformanceMark.
  await checkHasNoSideEffect(
      `performance.getEntriesByName('test-mark')[0].toJSON()`);
  await checkHasNoSideEffect(
      `JSON.stringify(performance.getEntriesByName('test-mark'))`);

  // Mutating timing methods still abort.
  await checkHasSideEffect(`performance.mark('blocked')`);
  await checkHasSideEffect(`performance.clearResourceTimings()`);

  testRunner.completeTest();

  async function checkHasSideEffect(expression) {
    return checkExpression(expression, true);
  }

  async function checkHasNoSideEffect(expression) {
    return checkExpression(expression, false);
  }

  // V8 appends a stack trace to exception.description, so match with startsWith
  // and fail explicitly on non-EvalError exceptions such as TypeError.
  async function checkExpression(expression, expectSideEffect) {
    const {result: {exceptionDetails}} =
        await dp.Runtime.evaluate({expression, throwOnSideEffect: true});
    let hasSideEffect = false;
    if (exceptionDetails) {
      const description =
          exceptionDetails.exception?.description ?? exceptionDetails.text;
      if (!description.startsWith(
              'EvalError: Possible side-effect in debug-evaluate')) {
        testRunner.log(
            `FAIL: Unexpected exception for \`${expression}\`: ${description}`);
        return;
      }
      hasSideEffect = true;
    }
    const failed = hasSideEffect !== expectSideEffect;
    testRunner.log(`${failed ? 'FAIL: ' : ''}Expression \`${expression}\`\n` +
        `has side effect: ${hasSideEffect}, expected: ${expectSideEffect}`);
  }
})
