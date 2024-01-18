(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const HTML = '<body><p class="class1" attr1="attr1">Paragraph Text</p></body>';
  const { dp, page } = await testRunner.startBlank('Tests the DOM.getAttributes API.');

  async function expectGetAttributesForElementWorksCorrectly() {
    // Tests getAttributes on Element node
    await page.loadHTML(HTML);

    const message = await dp.DOM.getDocument({ depth: -1 });
    const rootNode = message.result.root; // #document
    const targetNode = rootNode
      .children[0]  // <html>; doctype tag omitted
      .children[1]  // <body>; head tag implied
      .children[0]; // p.class[attr1]
    testRunner.log(targetNode);

    const nodeAttributes = await dp.DOM.getAttributes({ nodeId: targetNode.nodeId });
    testRunner.log(nodeAttributes.result.attributes);
  }

  async function expectGetAttributesOnNonElementReturnsError() {
    // Test that getAttributes returns error for non Element node
    await page.loadHTML(HTML);

    const documentMessage = await dp.DOM.getDocument();
    testRunner.log(documentMessage.result.root);

    const nodeId = documentMessage.result.root.nodeId;
    const docNodeAttributeMessage = await dp.DOM.getAttributes({ nodeId });
    testRunner.log(docNodeAttributeMessage.error);
  }

  testRunner.runTestSuite([
    expectGetAttributesForElementWorksCorrectly,
    expectGetAttributesOnNonElementReturnsError,
  ]);
})
