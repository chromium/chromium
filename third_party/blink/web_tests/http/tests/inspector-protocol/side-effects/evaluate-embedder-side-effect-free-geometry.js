(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that read-only geometry methods are side-effect-free.');

  await session.evaluate(`
    var div = document.createElement('div');
    div.style.width = '20px';
    div.style.height = '10px';
    document.body.appendChild(div);
    var quad = DOMQuad.fromQuad({p1: {x: 1, y: 2}});
    var matrix = DOMMatrix.fromMatrix({a: 2, e: 10});
    var point = DOMPointReadOnly.fromPoint({x: 1, y: 2});
    var f32 = new Float32Array([1, 0, 0, 1, 10, 20]);
    var f64 = new Float64Array([1, 0, 0, 1, 10, 20]);
  `);

  await checkHasNoSideEffect('div.getClientRects().item(0)');
  await checkHasNoSideEffect('DOMRect.fromRect({width: 10}).toJSON()');
  await checkHasNoSideEffect('DOMRectReadOnly.fromRect({width: 10}).toJSON()');
  await checkHasNoSideEffect('DOMPoint.fromPoint({x: 1}).toJSON()');
  await checkHasNoSideEffect('DOMPointReadOnly.fromPoint({x: 1}).toJSON()');
  await checkHasNoSideEffect(
      'JSON.stringify(DOMPointReadOnly.fromPoint({x: 1, y: 2}))');
  await checkHasNoSideEffect('point.matrixTransform({a: 2, e: 10})');
  await checkHasNoSideEffect('DOMMatrix.fromMatrix({a: 2, e: 10}).toJSON()');
  await checkHasNoSideEffect(
      'DOMMatrixReadOnly.fromMatrix({a: 2, e: 10}).toJSON()');
  await checkHasNoSideEffect(
      'JSON.stringify(DOMMatrixReadOnly.fromMatrix({a: 2, e: 10}))');
  await checkHasNoSideEffect('DOMMatrix.fromFloat32Array(f32)');
  await checkHasNoSideEffect('DOMMatrix.fromFloat64Array(f64)');
  await checkHasNoSideEffect('DOMMatrixReadOnly.fromFloat32Array(f32)');
  await checkHasNoSideEffect('DOMMatrixReadOnly.fromFloat64Array(f64)');
  await checkHasNoSideEffect('matrix.translate(1, 2)');
  await checkHasNoSideEffect('matrix.scale(2)');
  await checkHasNoSideEffect('matrix.scaleNonUniform(2, 3)');
  await checkHasNoSideEffect('matrix.scale3d(2)');
  await checkHasNoSideEffect('matrix.rotate(45)');
  await checkHasNoSideEffect('matrix.rotateFromVector(1, 1)');
  await checkHasNoSideEffect('matrix.rotateAxisAngle(0, 0, 1, 45)');
  await checkHasNoSideEffect('matrix.skewX(10)');
  await checkHasNoSideEffect('matrix.skewY(10)');
  await checkHasNoSideEffect('matrix.multiply({a: 2, e: 5})');
  await checkHasNoSideEffect('matrix.flipX()');
  await checkHasNoSideEffect('matrix.flipY()');
  await checkHasNoSideEffect('matrix.inverse()');
  await checkHasNoSideEffect('matrix.transformPoint({x: 1, y: 2})');
  await checkHasNoSideEffect('matrix.toFloat32Array()');
  await checkHasNoSideEffect('matrix.toFloat64Array()');
  await checkHasNoSideEffect('matrix.toJSON()');
  await checkHasNoSideEffect('DOMQuad.fromRect({width: 5}).getBounds()');
  await checkHasNoSideEffect('DOMQuad.fromQuad({p1: {x: 1, y: 2}}).toJSON()');
  await checkHasNoSideEffect(
      'JSON.stringify(DOMQuad.fromQuad({p1: {x: 1, y: 2}}))');
  await checkHasNoSideEffect('quad.getBounds()');
  await checkHasNoSideEffect('quad.toJSON()');
  await checkHasNoSideEffect('JSON.stringify(quad)');

  await checkHasSideEffect('matrix.translateSelf(1, 2)');
  await checkHasSideEffect('matrix.setMatrixValue(\'translate(1px, 2px)\')');
  // [Affects=Nothing] marks only the C++ callback; V8 still rejects
  // mutating JS getters during WebIDL dictionary conversion.
  await checkHasSideEffect(
      'DOMRect.fromRect({ get width() { window.pwned = 1; return 10; } })');

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
