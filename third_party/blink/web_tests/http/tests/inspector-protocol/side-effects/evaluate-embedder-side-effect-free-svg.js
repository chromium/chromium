(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that SVGSVGElement.getElementById() and SVGGraphicsElement.getBBox() are side-effect-free.');

  await session.evaluate(`
    var svgEl = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    var svgPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    svgPath.setAttribute('id', 'svg-path');
    svgPath.setAttribute('d', 'M0 0 L10 10');
    svgEl.appendChild(svgPath);
    document.body.appendChild(svgEl);
  `);

  await checkExpression(`svgEl.getElementById('svg-path')`, false);
  await checkExpression(`svgPath.getBBox()`, false);

  await checkExpression(`svgEl.pauseAnimations()`, true);
  // SVGGraphicsElement#getCTM() remains unannotated, so V8 still rejects the
  // call under throwOnSideEffect.
  await checkExpression(`svgPath.getCTM()`, true);

  // [Affects=Nothing] marks only the C++ callback, so V8 still rejects
  // mutating toString() methods during WebIDL DOMString argument conversion.
  await checkExpression(
      `svgEl.getElementById({ toString() { window.pwned = 1; return 'svg-path'; } })`,
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
})
