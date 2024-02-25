(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('resources/inline-wrap.html', 'Tests DOM.getContentQuads method.');

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  await dp.DOM.enable();

  const document = (await dp.DOM.getDocument()).result.root;
  const node = (await dp.DOM.querySelector({nodeId: document.nodeId, selector: 'span'})).result;
  const quads = (await dp.DOM.getContentQuads({nodeId: node.nodeId})).result.quads;
  testRunner.log('Returned quads amount: ' + quads.length);
  for (let i = 0; i < quads.length; ++i) {
    const quad = quads[i];
    const nodeId = (await dp.DOM.getNodeForLocation(middlePoint(quad))).result.nodeId;
    const node = nodeTracker.nodeForId(nodeId);
    testRunner.log(`node at quad #${i}: ${node.nodeName}`);
  }

  testRunner.completeTest();

  function middlePoint(quad) {
    let x = 0, y = 0;
    for (let i = 0; i < 8; i += 2) {
      x += quad[i];
      y += quad[i + 1];
    }
    return {
      x: Math.round(x / 4),
      y: Math.round(y / 4)
    };
  }
})

