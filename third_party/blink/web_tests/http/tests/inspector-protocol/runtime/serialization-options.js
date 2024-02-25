(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startHTML(
    '<div some_attr_name="some_attr_value">some text<h2>some another text</h2></div>',
    'Tests `serialization` options.');

  const ALL_TEST_LOGS = [];

  function testExpression(expression) {
    scheduleTest(expression, {
      serialization: 'deep',
    });
    scheduleTest(expression, {
      serialization: 'deep',
      maxDepth: 0,
    });
    scheduleTest(expression, {
      serialization: 'deep',
      maxDepth: 1,
    });
    scheduleTest(expression, {
      serialization: 'deep',
      maxDepth: 99,
    });
  }

  function scheduleTest(expression, serializationOptions) {
    ALL_TEST_LOGS.push(runTest(expression, serializationOptions));
  }

  async function runTest(expression, serializationOptions) {
    const evalResult = await dp.Runtime.evaluate({
      expression,
      serializationOptions,
    });
    const logs = [
      `Testing \`${expression}\` with ${JSON.stringify(serializationOptions)}`,
      evalResult?.result?.result?.deepSerializedValue ?? evalResult,
    ];
    return logs;
  }

  async function waitTestsDone() {
    for await (const logs of ALL_TEST_LOGS) {
      const [description, result] = logs;
      testRunner.log(description);
      testRunner.log(
        result,
        undefined,
        TestRunner.extendStabilizeNames(['context']),
      );
    }
  }

  await dp.Runtime.enable();

  // Test ECMAScript primitives.
  testExpression('undefined');
  testExpression('null');
  testExpression('"some_string"');
  testExpression('"2"');
  testExpression('Number.NaN');
  testExpression('-0');
  testExpression('Infinity');
  testExpression('-Infinity');
  testExpression('3');
  testExpression('1.4');
  testExpression('true');
  testExpression('false');
  testExpression('42n');
  testExpression('Symbol("foo")');
  // Test ECMAScript non-primitives.
  testExpression('[1, "foo", true, new RegExp(/foo/g), [1]]',);
  testExpression('({"foo": {"bar": "baz"}, "qux": "quux"})',);
  testExpression('(()=>{})');
  testExpression('(function(){})');
  testExpression('(async ()=>{})');
  testExpression('(async function(){})');
  testExpression('new RegExp(/foo/g)');
  testExpression('new Date(1654004849000)');
  testExpression('new Map([[1, 2], ["foo", "bar"], [true, false], ["baz", [1]]])',);
  testExpression('new Set([1, "foo", true, [1], new Map([[1,2]])])');
  testExpression('new WeakMap()');
  testExpression('new WeakSet()');
  testExpression('new Error("SOME_ERROR_TEXT")');
  testExpression('Promise.resolve()');
  testExpression('new Int32Array()');
  testExpression('new ArrayBuffer()');
  // Test non-ECMAScript platform objects.
  testExpression('document.body')
  testExpression('window')
  testExpression('document.querySelector("body > div")')
  testExpression('document.querySelector("body > div").attributes[0]')
  testExpression('new URL("http://example.com/")')

  await waitTestsDone();
  testRunner.completeTest();
});
