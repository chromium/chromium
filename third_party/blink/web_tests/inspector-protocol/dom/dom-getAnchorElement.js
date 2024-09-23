(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('resources/dom-get-anchor-element.html', 'Tests DOM.getAnchorElement command.');

  await dp.DOM.enable();
  const getDocumentResponse = await dp.DOM.getDocument();
  const target = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#target'})).result.nodeId;
  const defaultTarget = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#default-target'})).result.nodeId;

  // Get named, implicit, and default anchors via queries.
  const namedAnchorByQuery = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#named-anchor'})).result.nodeId;
  const implicitAnchorByQuery = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#implicit-anchor'})).result.nodeId;
  const defaultAnchorByQuery = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#default-anchor'})).result.nodeId;

  // Get named, implicit, and default anchors via DOM.getAnchorElement.
  const namedAnchor = await dp.DOM.getAnchorElement({nodeId: target, anchorSpecifier: '--anc'});
  const implicitAnchor = await dp.DOM.getAnchorElement({nodeId: target});
  const defaultAnchor = await dp.DOM.getAnchorElement({nodeId: defaultTarget});

  testRunner.log('Node Id from query selector and getAnchorElement should be the same:');
  testRunner.log(namedAnchorByQuery === namedAnchor.result.nodeId);
  testRunner.log(implicitAnchorByQuery === implicitAnchor.result.nodeId);
  testRunner.log(defaultAnchorByQuery === defaultAnchor.result.nodeId);

  const unreachableAnchor = await dp.DOM.getAnchorElement({nodeId: target, anchorSpecifier: '--unreachable'});
  const nonExistentAnchor = await dp.DOM.getAnchorElement({nodeId: namedAnchorByQuery});
  testRunner.log('Anchor query with no valid result should return node Id of 0:');
  testRunner.log(unreachableAnchor.result.nodeId);
  testRunner.log(nonExistentAnchor.result.nodeId);
  testRunner.completeTest();
})
