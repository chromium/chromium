(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that Range read-only methods and Selection.containsNode() are side-effect-free.');

  await session.evaluate(`
    var div = document.createElement('div');
    var connectedText = document.createTextNode('hello');
    div.appendChild(connectedText);
    document.body.appendChild(div);
    window.getSelection().selectAllChildren(div);
    var range = document.createRange();
    range.selectNodeContents(div);
  `);

  await checkHasNoSideEffect(
      'window.getSelection().containsNode(connectedText, true)');
  await checkHasNoSideEffect('range.cloneRange().getBoundingClientRect()');
  await checkHasNoSideEffect('range.getClientRects()');
  await checkHasNoSideEffect('range.intersectsNode(div)');
  await checkHasNoSideEffect('range.isPointInRange(connectedText, 0)');
  await checkHasNoSideEffect('range.comparePoint(connectedText, 0)');
  await checkHasNoSideEffect(
      'range.compareBoundaryPoints(Range.START_TO_START, range)');

  // getRangeAt() and getComposedRanges() remain unannotated because
  // populating SelectionEditor::cached_range_ alters subsequent rangeCount
  // checks when the selected element becomes display: none.
  await checkHasSideEffect('window.getSelection().getRangeAt(0)');
  await checkHasSideEffect('window.getSelection().getComposedRanges()');
  await checkHasSideEffect('range.collapse(true)');

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
