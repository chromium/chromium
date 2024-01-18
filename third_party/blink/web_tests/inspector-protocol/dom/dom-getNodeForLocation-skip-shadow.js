(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <form action='#'>
        <input type='text' style='position:absolute;top:0;left:0;width:100;height:100' />
    </form>
  `, 'Tests that DOM.getNodeForLocation method correctly skips shadow dom when instructed.');

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);

  var message = await dp.DOM.getNodeForLocation({x: 10, y: 10, includeUserAgentShadowDOM: false});
  var backendNodeId = message.result.backendNodeId;
  await dp.DOM.enable();
  await dp.DOM.getDocument();
  testRunner.log(await nodeTracker.nodeForBackendId(backendNodeId), 'Node: ');
  testRunner.completeTest();
})
