(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that CSS queries, Typed OM computedStyleMap, and font checks are side-effect free.');

  await session.evaluate(`
    document.body.style.margin = '0';
    document.body.innerHTML =
        '<style>@media screen { div { color: red; width: 100px; ' +
        'height: 100px; font-size: 20px; line-height: 20px; } }</style>' +
        '<div>footext</div>';
    var div = document.querySelector('div');
    // CaretPosition has no constructor, so create the receiver in setup to test
    // CaretPosition#getClientRect() independently of caretPositionFromPoint().
    var caretPos = document.caretPositionFromPoint(10, 10);
    if (!caretPos) {
      throw new Error('Expected non-null CaretPosition at (10, 10)');
    }
  `);

  // CSS namespace helpers.
  await checkHasNoSideEffect(`CSS.supports('display', 'grid')`);
  await checkHasNoSideEffect(`CSS.supports('(display: grid)')`);
  await checkHasNoSideEffect(`CSS.escape('foo.bar')`);

  // Typed OM computed style map and CSSOM CaretPosition.
  await checkHasNoSideEffect(`div.computedStyleMap().get('display')`);
  await checkHasNoSideEffect(`div.computedStyleMap().getAll('display')`);
  await checkHasNoSideEffect(`div.computedStyleMap().has('display')`);
  await checkHasNoSideEffect(`caretPos.getClientRect()`);

  // FontFaceSet check vs load.
  await checkHasNoSideEffect(`document.fonts.check('12px serif')`);
  await checkHasSideEffect(`document.fonts.load('12px serif')`);

  // CSS collection .item() vs mutator.
  await checkHasNoSideEffect(`document.styleSheets.item(0)`);
  await checkHasNoSideEffect(`document.styleSheets.item(0).cssRules.item(0)`);
  await checkHasNoSideEffect(
      `document.styleSheets.item(0).cssRules.item(0).media.item(0)`);
  await checkHasSideEffect(
      `document.styleSheets.item(0).cssRules.item(0).media.appendMedium('print')`);

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
