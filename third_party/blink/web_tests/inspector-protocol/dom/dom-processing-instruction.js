(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
    <div id='main'><?instruction attribute="value"?></div>
  `,
      'Test that processing instructions can be read and modified.');

  let response = await dp.DOM.getDocument({depth: -1});
  const {nodeId, nodeType, nodeName, localName, nodeValue} =
      response.result.root.children[0].children[1].children[0].children[0];

  testRunner.log({nodeType, nodeName, localName, nodeValue});

  await dp.DOM.setNodeValue(
      {nodeId, name: ' ', value: 'newAttribute="newValue"'});

  await logInnerHTML();

  await dp.DOM.setNodeName({nodeId, name: 'marker'});

  await logInnerHTML();

  testRunner.completeTest();

  async function logInnerHTML() {
    const evalResponse = await dp.Runtime.evaluate(
        {expression: 'document.getElementById("main").innerHTML'});
    testRunner.log(evalResponse.result.result.value);
  }
});
