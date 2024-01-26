
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
    `<div id='div'></div>`,
    'Tests that DOM.describeNode returns a nodeId iff one is currently assigned.');

  function nodeId(description) {
    const nodeId = description.node.nodeId;
    if (typeof nodeId !== 'number')
      return nodeId;
    return nodeId !== 0 ? 'present' : '0';
  }

  const {result} = await dp.Runtime.evaluate({expression: `document.getElementById('div')`});
  const objectId = result.result.objectId;
  let description = (await dp.DOM.describeNode({objectId})).result;

  testRunner.log(`NodeId before DOM agent enabled: ${nodeId(description)}`);

  await dp.DOM.getDocument();

  description = (await dp.DOM.describeNode({objectId})).result;
  testRunner.log(`NodeId after DOM.getDocument: ${nodeId(description)}`);

  const requestedNode = (await dp.DOM.requestNode({objectId})).result;

  description = (await dp.DOM.describeNode({objectId})).result;
  testRunner.log(`NodeId after node requested: ${nodeId(description)}`);

  testRunner.completeTest();
})
