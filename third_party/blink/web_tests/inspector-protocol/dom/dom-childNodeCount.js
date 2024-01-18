(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
        <div id='container' style='display:none'><div>child1</div><div>child2</div></div>
  `, 'Tests how DOM.childNodeCountUpdated event works.');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  var containerNodeId;
  dp.DOM.onChildNodeCountUpdated(message => {
    if (message.params.nodeId === containerNodeId)
      testRunner.log('childCountUpdated: ' + message.params.childNodeCount);
  });
  var response = await dp.DOM.getDocument();
  var message = await dp.DOM.querySelector({nodeId: response.result.root.nodeId, selector: '#container' });

  containerNodeId = message.result.nodeId;
  testRunner.log('Node arrived with childNodeCount: ' + nodeTracker.nodeForId(containerNodeId).childNodeCount);

  await Promise.all([
    session.evaluate(addNode),
    session.evaluate(removeNode),
    session.evaluate(removeNode),
    session.evaluate(removeNode),
  ]);
  testRunner.completeTest();

  function addNode() {
    var container = document.getElementById('container');
    container.appendChild(document.createElement('div'));
  }

  function removeNode() {
    var container = document.getElementById('container');
    container.firstChild.remove();
  }
})
