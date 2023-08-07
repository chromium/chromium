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

  await dp.Runtime.enable();

  // Node.
  await testExpression("document.querySelector('body > div')")
  // NodeList.
  await testExpression("document.querySelector('body > div').childNodes")
  // HTMLCollection.
  await testExpression("document.getElementsByTagName('div')")
  await testExpression("window")
  await testExpression("new URL('http://example.com')");
  await testExpression("window.openShadowContainer")
  await testExpression("window.closedShadowContainer")

  testRunner.completeTest();

  async function testExpression(expression) {
    await test(expression);
    await test(expression, { UNKNOWN_PARAMETER_NAME: 'SOME_VALUE' });
    await test(expression, { maxNodeDepth: 'STRING_INSTEAD_OF_INT' });
    await test(expression, { INVALID_PARAMETER_TYPE: {} });
    await test(expression, { maxNodeDepth:0 });
    await test(expression, { maxNodeDepth:1 });
    await test(expression, { maxNodeDepth:99 });
    await test(expression, {
      maxNodeDepth: 99,
      includeShadowTree: "none"
    }
    );
    await test(expression, {
      maxNodeDepth:99 ,
      includeShadowTree:"open"
    });
    await test(expression, {
      includeShadowTree: "all"
    }    );
    await test(expression, {
      maxNodeDepth: 0,
      includeShadowTree: "all"

    }    );
    await test(expression, {
      maxNodeDepth: 1,
      includeShadowTree: "all"

    });
    await test(expression, {
      maxNodeDepth: 99,
      includeShadowTree: "all"
    });
  }

  async function test(expression, additionalParameters) {
    const serializationOptions = {
      serialization: "deep",
        additionalParameters
    }
    testRunner.log(`Testing '${expression}' with ${JSON.stringify(serializationOptions)}`);

    const evalResult = await dp.Runtime.evaluate({
      expression,
      serializationOptions
    })
    testRunner.log(
      evalResult?.result?.result?.deepSerializedValue ?? evalResult,
      undefined,
      TestRunner.extendStabilizeNames(['context']),
    );
  }
})
