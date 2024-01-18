(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Testing that the Node ID can be retrieved before the onload event is triggered');
    /*
     1. Create an iframe and point it to a page with a debugger statement.
     2. Wait until the debugger statement is hit and pause using the inspector protocol.
     3. Use the JS context to identify the 'window.document' object inside the iframe.
     4. Use the JS object to retrieve the DOM agent nodeId for the document node.
     */
  dp.Page.onLoadEventFired(() => {
    // We should finish the test before this event is triggered.
    // If you see this in the output, then the slow-image is not loaded correctly in the iframe.
    testRunner.log('FAIL: Iframe load event fired before the test finished.');
    testRunner.completeTest();
  });
  testRunner.log('step1_bootstrap');
  // Enable Page agent for the Page.loadEventFired event.
  dp.Page.enable();
  // Make sure the debugger ready to break on the 'debugger' statement.
  dp.Debugger.enable();
  await dp.DOM.getDocument();
  testRunner.log('Adding iframe');
  session.evaluate(`
    var frame = document.createElement('iframe');
    frame.src = 'data:text/html,<script>debugger;</script>';
    document.body.appendChild(frame);
  `);
  var response = await dp.Debugger.oncePaused();
  testRunner.log('Paused on the debugger statement');
  response = await dp.Runtime.callFunctionOn({
    objectId: response.params.callFrames[0].this.objectId,
    functionDeclaration: 'function() { return this.document; }'
  });
  var objectId = response.result.result.objectId;
  testRunner.log('step2_requestNode: Requesting DOM node for iframe\'s document node');
  response = await dp.DOM.requestNode({objectId: objectId});
  testRunner.log(response.result.nodeId ? 'PASS: Received node for iframe\'s document node' : 'FAIL: Iframe\'s document node is not available');
  await dp.Debugger.resume();
  testRunner.log('Test finished');
  testRunner.completeTest();
})
