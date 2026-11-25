(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that btoa(), atob(), and NavigatorUAData.toJSON() are side-effect-free.');

  await checkHasNoSideEffect(`btoa('hello')`);
  await checkHasNoSideEffect(`atob('aGVsbG8=')`);
  await checkHasNoSideEffect(`navigator.userAgentData.toJSON()`);
  await checkHasNoSideEffect(`JSON.stringify(navigator.userAgentData)`);

  // getHighEntropyValues() remains blocked because it allocates a Promise,
  // posts an async task, and can report permissions-policy violations.
  await checkHasSideEffect(
      `navigator.userAgentData.getHighEntropyValues(['architecture'])`);

  // [Affects=Nothing] marks only the C++ callback, so V8 still rejects
  // mutating toString() methods during WebIDL DOMString argument conversion.
  await checkHasSideEffect(
      `btoa({ toString() { window.pwned = 1; return 'hello'; } })`);

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
});
