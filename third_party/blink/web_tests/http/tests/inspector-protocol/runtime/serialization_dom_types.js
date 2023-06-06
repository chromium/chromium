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
  }

  async function test(expression) {
    const serializationOptions = {
      serialization: "deep"
    }
    testRunner.log(`Testing '${expression}' with ${JSON.stringify(serializationOptions)}`);

    const evalResult = await dp.Runtime.evaluate({
      expression,
      serializationOptions
    })
    testRunner.log(evalResult?.result?.result?.deepSerializedValue ?? evalResult);
  }
})
