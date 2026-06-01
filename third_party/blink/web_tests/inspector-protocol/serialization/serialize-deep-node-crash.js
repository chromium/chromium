(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <div id="root"></div>
    <script>
    function buildDeepNode(depth) {
      const root = document.getElementById('root');
      let cur = root;
      for (let i = 0; i < depth; i++) {
        const child = document.createElement('div');
        cur.appendChild(child);
        cur = child;
      }
      return root;
    }

    const objX = {};
    const deepNode = buildDeepNode(400);
    const inner = new Set([objX, deepNode]);

    window.__x = [inner, objX];
    </script>
  `, 'Tests that serialization of deeply nested objects along with duplicates does not crash.');

  testRunner.log('Evaluating window.__x with deep serialization');
  const result = await dp.Runtime.evaluate({
    expression: 'window.__x',
    serializationOptions: {
      serialization: 'deep',
      additionalParameters: {
        maxNodeDepth: 500
      }
    }
  });

  if (result.error) {
    testRunner.log('Error: ' + result.error.message);
  } else if (result.result.exceptionDetails) {
    testRunner.log('Exception: ' + result.result.exceptionDetails.text);
  } else {
    testRunner.log('Success.');
  }

  testRunner.completeTest();
})
