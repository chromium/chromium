(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('resources/dom-request-child-nodes-traverse-frames.html', 'Tests how DOM.requestChildNodes pierces through frames.');

  var response = await dp.DOM.getDocument();
  var rootId = response.result.root.children[0].children[1].nodeId;
  dp.DOM.requestChildNodes({nodeId: rootId, depth: -1});
  var message = await dp.DOM.onceSetChildNodes();
  var iframeContentDocument = message.params.nodes[0].children[0].children[0].children[0].contentDocument;
  if (iframeContentDocument.children) {
    testRunner.die("Error IFrame node should not include children: " + JSON.stringify(iframeContentDocument, null, "    "));
    return;
  }
  var message = await dp.DOM.getDocument({pierce: true});
  var bodyId = message.result.root.children[0].children[1].nodeId;
  dp.DOM.requestChildNodes({nodeId: bodyId, depth: -1, pierce: true});
  var message = await dp.DOM.onceSetChildNodes();

  testRunner.log(message);
  testRunner.completeTest();
});
