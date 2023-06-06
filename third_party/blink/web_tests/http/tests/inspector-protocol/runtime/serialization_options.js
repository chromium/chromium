(async function (testRunner) {
  const { dp, session } = await testRunner.startHTML(
    '<div some_attr_name="some_attr_value">some text<h2>some another text</h2></div>',
    'Tests `serialization` options.');

  await dp.Runtime.enable();

  // Test V8 primitives.
  await testExpression("undefined");
  await testExpression("null");
  await testExpression("'some_string'");
  await testExpression("'2'");
  await testExpression("Number.NaN");
  await testExpression("-0");
  await testExpression("Infinity");
  await testExpression("-Infinity");
  await testExpression("3");
  await testExpression("1.4");
  await testExpression("true");
  await testExpression("false");
  await testExpression("42n");
  // Test V8 non-primitives.
  await testExpression("(Symbol('foo'))");
  await testExpression("[1, 'foo', true, new RegExp(/foo/g), [1]]",);
  await testExpression("({'foo': {'bar': 'baz'}, 'qux': 'quux'})",);
  await testExpression("(()=>{})");
  await testExpression("(function(){})");
  await testExpression("(async ()=>{})");
  await testExpression("(async function(){})");
  await testExpression("new RegExp(/foo/g)");
  await testExpression("new Date(1654004849000)");
  await testExpression("new Map([[1, 2], ['foo', 'bar'], [true, false], ['baz', [1]]])",);
  await testExpression("new Set([1, 'foo', true, [1], new Map([[1,2]])])");
  await testExpression("new WeakMap()");
  await testExpression("new WeakSet()");
  await testExpression("new Error('SOME_ERROR_TEXT')");
  await testExpression("Promise.resolve()");
  await testExpression("new Int32Array()");
  await testExpression("new ArrayBuffer()");
  // Test DOM objects.
  await testExpression("document.body")
  await testExpression("window")
  await testExpression("document.querySelector('body > div')")
  await testExpression("document.querySelector('body > div').attributes[0]")
  await testExpression("new URL('http://example.com')")

  testRunner.completeTest();

  async function testExpression(expression) {
    await test(expression, {
      serialization: "deep"
    });
    await test(expression, {
      serialization: "deep",
      maxDepth: 0
    });
    await test(expression, {
      serialization: "deep",
      maxDepth: 1
    });
    await test(expression, {
      serialization: "deep",
      maxDepth: 99
    });
  }

  async function test(expression, serializationOptions) {
    testRunner.log(`Testing '${expression}' with ${JSON.stringify(serializationOptions)}`);

    const evalResult = await dp.Runtime.evaluate({
      expression,
      serializationOptions
    })
    testRunner.log(evalResult.result.result.deepSerializedValue);
  }
})
