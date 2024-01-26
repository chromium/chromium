const HTML = `<body>
  <div class="testClass" id="firstDiv"></div>
  <div class="testClass" id="secondDiv"></div>
  <div class="testClass"></div>
  <div class="testClass"></div>
  <div class="testClass"></div>

  <div id="depth-1">
    <div id="depth-2">
      <div id="targetDiv"></div>
    </div>
    <div id="targetUncle">
      <div id="targetCousin"></div>
    </div>
  </div>
</body>`;

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { findBodyNode, searchNodesByAttributeValue, searchNodesByNodeId } =
    await testRunner.loadScript('resources/dom-test-helper.js');

  testRunner.log('Tests DOM.querySelector and DOM.querySelectorAll');
  const { dp, page } = await testRunner.startBlank('querySelector: by node name');

  async function getFirstDivInBody() {
    // Test 1: query for the first div in the body
    await page.loadHTML(HTML);
    const bodyNode = await findBodyNode(dp);
    testRunner.log(bodyNode);

    const [queryMessage, setChildNodesResponse] = await Promise.all([
      dp.DOM.querySelector({ nodeId: bodyNode.nodeId, selector: 'div' }),
      dp.DOM.onceSetChildNodes(),
    ]);
    testRunner.log(queryMessage.result);

    const targetDivNode = searchNodesByNodeId(setChildNodesResponse.params.nodes, queryMessage.result.nodeId);
    testRunner.log(targetDivNode);
  }

  async function queryForSecondDivInBodyById() {
    // Test 2: querySelector for nodeName#id
    await page.loadHTML(HTML);
    const bodyNode = await findBodyNode(dp);
    testRunner.log(bodyNode);

    const [queryMessage, setChildNodesResponse] = await Promise.all([
      dp.DOM.querySelector({ nodeId: bodyNode.nodeId, selector: 'div#secondDiv' }),
      dp.DOM.onceSetChildNodes(),
    ]);
    testRunner.log(queryMessage.result);

    const targetDivNode = searchNodesByNodeId(setChildNodesResponse.params.nodes, queryMessage.result.nodeId);
    testRunner.log(targetDivNode.attributes);
  }

  async function queryForAllDivsWithGivenClassName() {
    // Test 3: query for all divs with a class=testClass
    await page.loadHTML(HTML);
    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const bodyNode = await findBodyNode(dp);
    testRunner.log(bodyNode);

    const queryMessage = await dp.DOM.querySelectorAll({ nodeId: bodyNode.nodeId, selector: 'div.testClass' });
    testRunner.log(queryMessage.result.nodeIds.length);
  }

  async function expectProperSetChildNodeEventsForDeepNodeQuery() {
    // Test 4: Ensure that a query for a deeper node fires the correct setChildEvents
    await page.loadHTML(HTML);
    const setChildNodesResponses = [];
    dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));

    const bodyNode = await findBodyNode(dp);
    testRunner.log(bodyNode);

    const queryMessage = await dp.DOM.querySelector({ nodeId: bodyNode.nodeId, selector: 'div#targetDiv' });
    testRunner.log(queryMessage.result);

    for (let response of setChildNodesResponses) {
      const cousinDiv = searchNodesByAttributeValue(response.params.nodes, 'cousinDiv');
      if (cousinDiv) {
        testRunner.fail('A DOM.setChildNodes event was fired for a non direct parent of the target div.');
        break;
      }
    }
  }

  testRunner.runTestSuite([
    getFirstDivInBody,
    queryForSecondDivInBodyById,
    queryForAllDivsWithGivenClassName,
    expectProperSetChildNodeEventsForDeepNodeQuery,
  ]);

})
