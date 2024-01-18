(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div style='position:absolute;top:0;left:0;width:100;height:100'></div>
    <div style='position:absolute;top:0;left:0;width:200;height:200;pointer-events:none'></div>
  `, 'Tests DOM.getNodeForLocation method.');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);

  var response = await dp.DOM.getNodeForLocation({x: 10, y: 10});
  var backendNodeId = response.result.backendNodeId;
  await dp.DOM.enable();
  await dp.DOM.getDocument();
  testRunner.log(await nodeTracker.nodeForBackendId(backendNodeId), 'Node: ');

  await dp.Page.enable();
  var response2 = await dp.Page.getFrameTree({});
  testRunner.log('FrameID matched: ' + (response2.result.frameTree.frame.id === response.result.frameId));

  testRunner.completeTest();
})
