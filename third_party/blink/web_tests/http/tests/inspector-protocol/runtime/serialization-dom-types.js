(async function (testRunner) {
  const { dp, session } = await testRunner.startHTML(
    `<div some_attr_name="some_attr_value">some text<h2>some another text</h2></div>
     <script>
      function createShadow(mode) {
        // Create container element.
        const shadowContainer = document.createElement('div');
        document.body.appendChild(shadowContainer);
        // Create a closed shadow DOM.
        const shadowRoot = shadowContainer.attachShadow({ mode });
        // Create another element.
        const shadowElement = document.createElement('div');
        shadowElement.innerHTML = \`element in \${mode} shadow DOM\`;
        // Attach shadow element to the shadow DOM.
        shadowRoot.appendChild(shadowElement);
        return shadowContainer;
      }
      window.openShadowContainer = createShadow('open');
      window.closedShadowContainer = createShadow('closed');
     </script>
    `,
    'Tests DOM objects serialization');

  const ALL_TEST_LOGS = [];

  function testExpression(expression) {
    scheduleTest(expression);
    scheduleTest(expression, { UNKNOWN_PARAMETER_NAME: 'SOME_VALUE' });
    scheduleTest(expression, { maxNodeDepth: 'STRING_INSTEAD_OF_INT' });
    scheduleTest(expression, { INVALID_PARAMETER_TYPE: {} });
    scheduleTest(expression, { maxNodeDepth: 0 });
    scheduleTest(expression, { maxNodeDepth: 1 });
    scheduleTest(expression, { maxNodeDepth: 99 });
    scheduleTest(expression, {
      maxNodeDepth: 99,
      includeShadowTree: 'none'
    }
    );
    scheduleTest(expression, {
      maxNodeDepth: 99,
      includeShadowTree: 'open'
    });
    scheduleTest(expression, {
      includeShadowTree: 'all'
    });
    scheduleTest(expression, {
      maxNodeDepth: 0,
      includeShadowTree: 'all'
    });
    scheduleTest(expression, {
      maxNodeDepth: 1,
      includeShadowTree: 'all'

    });
    scheduleTest(expression, {
      maxNodeDepth: 99,
      includeShadowTree: 'all'
    });
  }

  function scheduleTest(expression, additionalParameters) {
    ALL_TEST_LOGS.push(runTest(expression, additionalParameters));
  }

  async function runTest(expression, additionalParameters) {
    const serializationOptions = {
      serialization: 'deep',
      additionalParameters
    }

    const evalResult = await dp.Runtime.evaluate({
      expression,
      serializationOptions
    })

    return [
      `Testing '${expression}' with ${JSON.stringify(serializationOptions)}`,
      evalResult?.result?.result?.deepSerializedValue ?? evalResult]
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

  // Node.
  testExpression('document.querySelector("body > div")')
  // NodeList.
  testExpression('document.querySelector("body > div").childNodes')
  // HTMLCollection.
  testExpression('document.getElementsByTagName("div")')
  testExpression('window')
  testExpression('new URL("http://example.com")');
  testExpression('window.openShadowContainer')
  testExpression('window.closedShadowContainer')

  await waitTestsDone();
  testRunner.completeTest();
})
