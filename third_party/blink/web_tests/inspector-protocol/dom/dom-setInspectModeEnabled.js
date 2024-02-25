(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div style="position:absolute;top:100;left:100;width:100;height:100;background:black"></div>
  `, 'Tests inspect mode.');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  dp.DOM.enable();
  dp.Overlay.enable();
  await dp.DOM.getDocument();
  var message = await dp.Overlay.setInspectMode({ mode: 'searchForNode', highlightConfig: {} });
  if (message.error) {
    testRunner.die(message.error.message);
    return;
  }
  const highlightRequests = [];
  dp.Overlay.onNodeHighlightRequested(event => highlightRequests.push(event));
  dp.Input.dispatchMouseEvent({type: 'mouseMoved', button: 'left', buttons: 0, clickCount: 1, x: 175, y: 175 });
  dp.Input.dispatchMouseEvent({type: 'mouseMoved', button: 'left', buttons: 0, clickCount: 1, x: 150, y: 150 });
  dp.Input.dispatchMouseEvent({type: 'mousePressed', button: 'left', buttons: 0, clickCount: 1, x: 150, y: 150 });
  dp.Input.dispatchMouseEvent({type: 'mouseReleased', button: 'left', buttons: 1, clickCount: 1, x: 150, y: 150 });

  var message = await dp.Overlay.onceInspectNodeRequested();
  message = await dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds: [message.params.backendNodeId]});
  testRunner.log('DOM.inspectNodeRequested: ' + nodeTracker.nodeForId(message.result.nodeIds[0]).localName);
  testRunner.log('Number of nodeHighlightRequested events: ' + highlightRequests.length);
  testRunner.log('Overlay.nodeHighlightRequested: ' + nodeTracker.nodeForId(highlightRequests[0].params.nodeId).localName);
  testRunner.completeTest();
})

