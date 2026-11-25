(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that URL and URLSearchParams read-only methods are side-effect-free.');

  // Obtain URLSearchParams from URL.parse() because URL and URLSearchParams
  // constructors lack [Affects=Nothing].
  await checkHasNoSideEffect(`URL.canParse('https://example.com/?a=1')`);
  await checkHasNoSideEffect(`URL.parse('https://example.com/?a=1').toJSON()`);
  await checkHasNoSideEffect(
      `JSON.stringify(URL.parse('https://example.com/?a=1'))`);
  await checkHasNoSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.get('a')`);
  await checkHasNoSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.getAll('a')`);
  await checkHasNoSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.has('a')`);

  // URLSearchParams mutators remain blocked.
  await checkHasSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.set('a', '2')`);
  await checkHasSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.append('b', '3')`);
  await checkHasSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.delete('a')`);
  await checkHasSideEffect(
      `URL.parse('https://example.com/?a=1').searchParams.sort()`);

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
