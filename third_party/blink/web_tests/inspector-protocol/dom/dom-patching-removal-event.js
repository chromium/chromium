(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank('Test that processing instructions removal is reported to DevTools during patching.');

  await dp.DOM.enable();

  // 1. Setup initial document with markers.
  await dp.Runtime.evaluate({expression: `
    document.open();
    document.write('<div id="main"><?start name="counter"?>0<?end?></div>');
  `});

  // 2. Get the document and identify the markers.
  const {result: {root}} = await dp.DOM.getDocument({depth: -1});
  // root -> html -> body -> div#main
  const body = root.children[0].children[1];
  const main = body.children[0];
  const startMarkerId = main.children[0].nodeId;
  const contentId = main.children[1].nodeId;
  const endMarkerId = main.children[2].nodeId;

  testRunner.log('Initial markers identified.');

  // 3. Listen for removal and insertion events.
  const removedNodes = new Set();
  dp.DOM.onChildNodeRemoved(message => {
    removedNodes.add(message.params.nodeId);
  });

  const insertedNodes = [];
  dp.DOM.onChildNodeInserted(message => {
    insertedNodes.push(message.params.node.nodeName);
  });

  // 4. Apply the patch with new markers.
  testRunner.log('Applying patch with new markers...');
  await dp.Runtime.evaluate({expression: `
    document.write('<template for="counter"><?start name="counter"?>1<?end?></template>');
    document.close();
  `});

  // 5. Verify results.
  testRunner.log(`Start marker removal reported: ${removedNodes.has(startMarkerId)}`);
  testRunner.log(`Content removal reported: ${removedNodes.has(contentId)}`);
  testRunner.log(`End marker removal reported: ${removedNodes.has(endMarkerId)}`);

  testRunner.log('Inserted nodes names during patch:');
  for (const name of insertedNodes) {
    // Filter out some noise if any, but usually it should be the new nodes.
    if (name !== 'HTML' && name !== 'HEAD' && name !== 'BODY') {
      testRunner.log(`  - ${name}`);
    }
  }

  // Re-request document to verify final state.
  const {result: {root: rootAfter}} = await dp.DOM.getDocument({depth: -1});
  const bodyAfter = rootAfter.children[0].children[1];
  const mainAfter = bodyAfter.children[0];

  testRunner.log(`Main children count after patch: ${mainAfter.children.length}`);
  for (const child of mainAfter.children) {
    let info = child.nodeName;
    if (child.nodeValue) info += ` "${child.nodeValue}"`;
    testRunner.log(`  - ${info}`);
  }

  testRunner.completeTest();
});
