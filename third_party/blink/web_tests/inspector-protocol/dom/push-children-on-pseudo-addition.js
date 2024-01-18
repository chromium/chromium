(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style id='style'>
    </style>
    <div id='for-pseudo'><span id='inner-span'></span></div>
  `, 'Tests that DOM pushes child node updates on pseudo-element addition.');

  var DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);

  testRunner.log('\n=== Get the Document ===\n');
  var response = await dp.DOM.getDocument();
  var bodyId = response.result.root.children[0].children[1].nodeId;

  testRunner.log('\n=== Get immediate children of the body ===\n');
  dp.DOM.requestChildNodes({nodeId: bodyId});
  await dp.DOM.onceSetChildNodes();

  testRunner.log('\n=== Add #for-pseudo:before element ===\n');
  session.evaluate(() => {
    document.getElementById('style').textContent = '#for-pseudo:before { content: "BEFORE" }';
  });
  await dp.DOM.oncePseudoElementAdded();

  for (var node of nodeTracker.nodes()) {
    if (DOMHelper.attributes(node).get('id') === 'inner-span') {
      testRunner.log('PASS: #inner-span has been received');
      testRunner.completeTest();
      return;
    }
  }
  testRunner.die('FAIL: #inner-span was not received');
})
