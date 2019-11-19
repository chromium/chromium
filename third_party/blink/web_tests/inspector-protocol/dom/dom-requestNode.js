(async function (testRunner) {
  // Needs to successively refer to an iframe relative to the source file, so
  // the test runner will launch multiple sessions against this URL.
  const testUrl = 'resources/dom-request-node-test.html';
  const { searchNodesByAttributeValue, searchNodesForContentDocument } =
    await testRunner.loadScript('resources/dom-test-helper.js');

  const { dp, page } = await testRunner.startBlank('Tests permutations of DOM.requestNode');

  async function requestTargetAfterTraversal() {
    // Test 1: request the targetDiv after DOM has already traversed it (no setChildNodes events)
    await page.navigate(testUrl);

    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const documentMessage = await dp.DOM.getDocument({ depth: -1, pierce: true });
    const root = documentMessage.result.root;
    const targetNode = searchNodesByAttributeValue(root.children, 'targetDiv');
    testRunner.log(targetNode);
    const targetNodeId = targetNode.nodeId;

    const evalMessage = await dp.Runtime.evaluate({ expression: `document.getElementById('targetDiv')` });
    testRunner.log(evalMessage.result);
    const targetRemoteObjectId = evalMessage.result.result.objectId;

    const requestMessage = await dp.DOM.requestNode({ objectId: targetRemoteObjectId });
    const requestedNodeId = requestMessage.result.nodeId;
    testRunner.log(`requested === targeted: ${requestedNodeId === targetNodeId}`);
  }

  async function requestTargetBeforeTraversal() {
    // Test 2: request the targetDiv before DOM has traversed it (fire setChildNodes events)
    await page.navigate(testUrl);
    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const documentMessage = await dp.DOM.getDocument({});
    const evalMessage = await dp.Runtime.evaluate({ expression: `document.getElementById('targetDiv')` });
    testRunner.log(evalMessage.result);

    const targetRemoteObjectId = evalMessage.result.result.objectId;
    const requestMessage = await dp.DOM.requestNode({ objectId: targetRemoteObjectId });
    testRunner.log(requestMessage.result);

    const requestedNodeId = requestMessage.result.nodeId;
    // Ensure correct setChildNodes events were fired.
    for (let response of setChildNodesResponses) {
      const cousinDiv = searchNodesByAttributeValue(response.params.nodes, 'cousinDiv');
      if (cousinDiv) {
        testRunner.fail('A DOM.setChildNodes event was fired for a non direct parent of the target div.');
        break;
      }
    }

    // Ensure the returned id matches the assigned node id
    const targetDiv = searchNodesByAttributeValue(setChildNodesResponses.pop().params.nodes, 'targetDiv');
    testRunner.log(targetDiv);
    testRunner.log(`requested === targeted: ${requestedNodeId === targetDiv.nodeId}`);
  }

  async function requestNodeByObjectId() {
    // Test 3: RequestNode on ObjectId that is not a node
    await page.navigate(testUrl);
    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const evalMessage = await dp.Runtime.evaluate({ expression: 'testObject' });
    testRunner.log(evalMessage.result);
    const targetRemoteObjectId = evalMessage.result.result.objectId;

    const requestMessage = await dp.DOM.requestNode({ objectId: targetRemoteObjectId });
    testRunner.log(requestMessage.error);
  }

  async function requestNodeInIframeByObjectId() {
    // Test 4: RequestNode on an ObjectId inside a subFrame that is not traversed yet
    await page.navigate(testUrl);
    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const documentMessage = await dp.DOM.getDocument({ depth: -1, pierce: true });
    const root = documentMessage.result.root;
    // Grab a target Id from an iframe
    const iframeDoc = searchNodesForContentDocument(root.children);
    testRunner.log(iframeDoc);
    const targetNode = searchNodesByAttributeValue(iframeDoc.children, 'class1');
    testRunner.log(targetNode);

    // Resolve the target nodeId to an objectId
    const targetNodeId = targetNode.nodeId;
    const resolvedMessage = await dp.DOM.resolveNode({ nodeId: targetNodeId });
    const resolvedObjectId = resolvedMessage.result.object.objectId;

    // reset the node Id tree, not traversing the iframe
    await dp.DOM.getDocument();
    const requestMessage = await dp.DOM.requestNode({ objectId: resolvedObjectId });
    testRunner.log(requestMessage.result);
  }

  testRunner.runTestSuite([
    requestTargetAfterTraversal,
    requestTargetBeforeTraversal,
    requestNodeByObjectId,
    requestNodeInIframeByObjectId,
  ]);
})
