(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startHTML(
      `
        <div id="non-popover">Not a popover</div>

        <div popover id="p-no-invoker">No invokers</div>

        <button id="btn-pt1" popovertarget="p-multiple"></button>
        <button id="btn-pt2" popovertarget="p-multiple"></button>
        <button id="btn-cf1" commandfor="p-multiple" command="show-popover"></button>
        <div popover id="p-multiple"></div>

        <button id="btn-programmatic"></button>
        <div popover id="p-programmatic"></div>

        <div id="host">
          <template shadowrootmode="open">
            <button id="btn-in-shadow"></button>
          </template>
        </div>
        <div popover id="p-shadow"></div>

        <div id="host-inner-popover">
          <template shadowrootmode="open">
            <button id="btn-for-inner-shadow" popovertarget="p-inside-shadow"></button>
            <div popover id="p-inside-shadow"></div>
          </template>
        </div>

        <button id="btn-for-ref-target" popovertarget="host-ref-target"></button>
        <div id="host-ref-target">
          <template shadowrootmode="open" shadowrootreferencetarget="p-ref-target">
            <div popover id="p-ref-target"></div>
          </template>
        </div>

        <div popover id="p-capped"></div>
        <div id="capped-container"></div>
    `,
      'Tests that DOM.getImplicitAnchorCandidates returns expected candidate node IDs');

  await dp.Runtime.enable();
  await dp.DOM.enable();
  const doc = await dp.DOM.getDocument({depth: -1, pierce: true});

  async function getNodeId(selector) {
    const {result: {nodeId}} =
        await dp.DOM.querySelector({nodeId: doc.result.root.nodeId, selector});
    return nodeId;
  }

  async function getElementDescription(backendNodeId) {
    const {result: {node}} = await dp.DOM.describeNode({backendNodeId});
    const idAttr = (node.attributes || []).indexOf('id');
    const id = idAttr !== -1 ? node.attributes[idAttr + 1] : '<no-id>';
    return `<${node.nodeName.toLowerCase()} id="${id}">`;
  }

  async function testCandidates(popoverNodeId, label) {
    testRunner.log(`\n--- ${label} ---`);
    const {result, error} =
        await dp.DOM.getImplicitAnchorCandidates({nodeId: popoverNodeId});
    if (error) {
      testRunner.log('Error: ' + error.message);
      return;
    }
    testRunner.log(`Candidate count: ${result.backendNodeIds.length}`);
    for (const id of result.backendNodeIds) {
      testRunner.log('  ' + await getElementDescription(id));
    }
  }

  // Setup programmatic invoker
  await dp.Runtime.evaluate({
    expression: `
      document.getElementById('btn-programmatic').popoverTargetElement =
          document.getElementById('p-programmatic');
    `
  });

  // Setup shadow DOM invoker targeting light DOM popover (not in popover's
  // scope or ancestor scopes)
  await dp.Runtime.evaluate({
    expression: `
      document.getElementById('host').shadowRoot.getElementById('btn-in-shadow').popoverTargetElement =
          document.getElementById('p-shadow');
    `
  });

  // Setup > 50 candidates for capping test
  await dp.Runtime.evaluate({
    expression: `
      const container = document.getElementById('capped-container');
      for (let i = 0; i < 55; ++i) {
        const btn = document.createElement('button');
        btn.id = 'btn-capped-' + i;
        btn.setAttribute('popovertarget', 'p-capped');
        container.appendChild(btn);
      }
    `
  });

  // Test 1: Popover with no invokers
  await testCandidates(await getNodeId('#p-no-invoker'),
                       'Popover with no invokers');

  // Test 2: Popover with multiple invokers (popovertarget and commandfor)
  await testCandidates(await getNodeId('#p-multiple'),
                       'Popover with multiple invokers');

  // Test 3: Programmatic invoker (popoverTargetElement property)
  await testCandidates(await getNodeId('#p-programmatic'),
                       'Programmatic invoker');

  // Test 4: Child tree scope invoker is not an ancestor scope of light DOM
  // popover
  await testCandidates(await getNodeId('#p-shadow'),
                       'Invoker in child tree scope is not returned');

  // Test 5: Popover inside shadow DOM invoked from the same shadow DOM
  const hostId = await getNodeId('#host-inner-popover');
  const {result: {node: hostNode}} =
      await dp.DOM.describeNode({nodeId: hostId, depth: -1, pierce: true});
  const shadowRootNodeId = hostNode.shadowRoots[0].nodeId;
  const {result: {nodeId: innerPopNodeId}} = await dp.DOM.querySelector(
      {nodeId: shadowRootNodeId, selector: '#p-inside-shadow'});
  await testCandidates(
      innerPopNodeId, 'Popover inside shadow DOM invoked from same shadow DOM');

  // Test 6: Popover inside shadow DOM invoked from parent scope via Reference
  // Target
  const refTargetHostId = await getNodeId('#host-ref-target');
  const {result: {node: refTargetHostNode}} = await dp.DOM.describeNode(
      {nodeId: refTargetHostId, depth: -1, pierce: true});
  const refTargetShadowRootNodeId = refTargetHostNode.shadowRoots[0].nodeId;
  const {result: {nodeId: refTargetPopNodeId}} = await dp.DOM.querySelector(
      {nodeId: refTargetShadowRootNodeId, selector: '#p-ref-target'});
  await testCandidates(
      refTargetPopNodeId,
      'Popover inside shadow DOM invoked from parent scope via Reference Target');

  // Test 7: Capping at 50 candidates
  testRunner.log('\n--- Capping at 50 candidates ---');
  const cappedPopoverId = await getNodeId('#p-capped');
  const {result: cappedResult} =
      await dp.DOM.getImplicitAnchorCandidates({nodeId: cappedPopoverId});
  testRunner.log(`Capped candidate count (expected 50): ${
      cappedResult.backendNodeIds.length}`);

  // Test 8: Error handling on non-popover element
  testRunner.log('\n--- Error handling: non-popover element ---');
  const nonPopoverId = await getNodeId('#non-popover');
  const {error: nonPopoverError} =
      await dp.DOM.getImplicitAnchorCandidates({nodeId: nonPopoverId});
  testRunner.log('Non-popover error: ' +
                 (nonPopoverError ? nonPopoverError.message : 'none'));

  // Test 9: Error handling on non-existent node ID
  testRunner.log('\n--- Error handling: non-existent node ID ---');
  const {error: invalidIdError} =
      await dp.DOM.getImplicitAnchorCandidates({nodeId: 999999});
  testRunner.log('Invalid node ID error: ' +
                 (invalidIdError ? invalidIdError.message : 'none'));

  testRunner.completeTest();
})
