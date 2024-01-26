(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('./resources/dom-request-document-with-child-nodes.html', 'Tests how DOM.getDocument reports all child nodes when asked.');
  var response = await dp.DOM.getDocument({depth: -1});
  var iframeOwner = response.result.root.children[0].children[1].children[0].children[0].children[0].children[0];
  if (iframeOwner.contentDocument.children) {
    testRunner.die('Error IFrame node should not include children: ' + JSON.stringify(iframeOwner, null, '    '));
    return;
  }

  var response = await dp.DOM.getDocument({depth: -1, pierce: true});
  testRunner.log(response);
  testRunner.completeTest();
})

