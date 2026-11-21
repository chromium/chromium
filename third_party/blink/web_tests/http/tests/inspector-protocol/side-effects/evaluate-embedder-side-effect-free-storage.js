(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that Storage.getItem() and Storage.key() are side-effect-free.');

  await session.evaluate(
      'localStorage.setItem(\'k\', \'v\'); sessionStorage.setItem(\'sk\', \'sv\');');

  // Storage#getItem() and Storage#key() match side-effect-free named property
  // reads (localStorage['k']), while Storage write methods remain blocked.
  await checkExpression('localStorage.getItem(\'k\')', false);
  await checkExpression('localStorage.key(0)', false);
  await checkExpression('sessionStorage.getItem(\'sk\')', false);
  await checkExpression('sessionStorage.key(0)', false);

  await checkExpression('localStorage.setItem(\'k\', \'v2\')', true);
  await checkExpression('localStorage.removeItem(\'k\')', true);
  await checkExpression('localStorage.clear()', true);
  await checkExpression('sessionStorage.setItem(\'sk\', \'sv2\')', true);

  // [Affects=Nothing] marks only the C++ callback, so V8 still rejects mutating
  // toString() methods during WebIDL DOMString key conversion.
  await checkExpression(
      'localStorage.getItem({ toString() { window.pwned = 1; return \'k\'; } })',
      true);

  testRunner.completeTest();

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
