(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { searchNodesByAttributeValue } = await testRunner.loadScript('resources/dom-test-helper.js');
  const testUrl = 'resources/dom-mutation-events-test.html';

  const { dp, session, page } = await testRunner.startBlank('Tests DOM mutation events');

  async function expectChildNodeUpdatedFiresForVisitedButNotTraversedNode() {
    // Test 1: check that childNodeCountUpdated fires on a visited, but not traversed node.
    await page.navigate(testUrl);

    const documentMessage = await dp.DOM.getDocument({ depth: 3 });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'childrenAddRemoveTarget');
    testRunner.log(targetNode);

    session.evaluate('addDivChild()');
    const childUpdatedMessage = await dp.DOM.onceChildNodeCountUpdated();
    testRunner.log(childUpdatedMessage.params);
    testRunner.log(`Child updated node ID = childrenAddRemoveTarget ID? ${childUpdatedMessage.params.nodeId === targetNode.nodeId}`);
  }

  async function expectChildNodeInsertedAndRemovedFiresOnTraversedNode() {
    // Test 2: check that childNodeInserted and childNodeRemoved fires on a traversed node.
    await page.navigate(testUrl);

    const documentMessage = await dp.DOM.getDocument({ depth: -1 });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'childrenAddRemoveTarget');
    const targetNodeId = targetNode.nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    session.evaluate('addDivChild()');
    const childNodeInsertedMessage = await dp.DOM.onceChildNodeInserted();
    testRunner.log(`Inserted node parent correct? ${childNodeInsertedMessage.params.parentNodeId === targetNodeId}`);

    session.evaluate('removeDivChild()');
    const childNodeRemovedMessage = await dp.DOM.onceChildNodeRemoved();
    testRunner.log(`Removed node parent correct? ${childNodeRemovedMessage.params.parentNodeId === targetNodeId}`);
  }

  async function expectCharacterDataModifiedFiredWhenDataModified() {
    // Test 3: Check that characterDataModified events fire when data is modified
    await page.navigate(testUrl);

    const documentMessage = await dp.DOM.getDocument({ depth: -1 });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'editDiv');
    const targetNodeId = targetNode.children[0].nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    session.evaluate('modifyData()');
    const charDataModMessage = await dp.DOM.onceCharacterDataModified();
    testRunner.log(`Modified node ID expected? ${charDataModMessage.params.nodeId === targetNodeId}`);
  }

  async function expectAttrModifiedAndRemovedFire() {
    // Test 4: Check that attributeModified and attributeRemovedEvents fire
    await page.navigate(testUrl);

    const documentMessage = await dp.DOM.getDocument({ depth: -1 });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'attrTarget');
    const targetNodeId = targetNode.nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    session.evaluate('addAttr()');
    const attrModMessage = await dp.DOM.onceAttributeModified();
    testRunner.log(`Modified node ID expected? ${attrModMessage.params.nodeId === targetNodeId}`);

    session.evaluate('removeAttr()');
    const attrRemovedMessage = await dp.DOM.onceAttributeRemoved();
    testRunner.log(`Removed attribute node ID expected? ${attrRemovedMessage.params.nodeId === targetNodeId}`);
  }

  async function expectChildNodeRemovedAndInsertedFireForIframeNavigation () {
    // Test 5: Check that childNodeRemoved and childNodeInserted fire for an iframe Navigation.
    await page.navigate(testUrl);

    const documentMessage = await dp.DOM.getDocument({ depth: -1 });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'frameContainer');
    const targetNodeId = targetNode.nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    await session.evaluate('addFrame()');
    const childNodeRemovedMessage = await dp.DOM.onceChildNodeRemoved();
    testRunner.log(`iframe child removed correct node? ${childNodeRemovedMessage.params.parentNodeId === targetNodeId}`);

    const childNodeInsertedMessage = await dp.DOM.onceChildNodeInserted();
    testRunner.log(`Inserted child node matches expected ID? ${childNodeInsertedMessage.params.parentNodeId === targetNodeId}`);
  }

  testRunner.runTestSuite([
    expectChildNodeUpdatedFiresForVisitedButNotTraversedNode,
    expectChildNodeInsertedAndRemovedFiresOnTraversedNode,
    expectCharacterDataModifiedFiredWhenDataModified,
    expectAttrModifiedAndRemovedFire,
    expectChildNodeRemovedAndInsertedFireForIframeNavigation,
  ]);

});
