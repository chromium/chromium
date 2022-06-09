(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <style> ::page-transition-incoming-image(shared) {animation-duration: 300s;} </style>
      <style> ::page-transition-outgoing-image(*) {background: red;} </style>
      <div style='width:100px; height:100px; page-transition-tag: shared; contain:paint;'></div>`,
      'The test verifies functionality of querying style information for document-transition pseudo elements');

  await session.evaluateAsync(`
     new Promise(resolve => {
       document.createDocumentTransition().start(resolve);
     });
  `);

  const response = await dp.DOM.getDocument();
  const rootNode = response.result.root.children[0];

  await dp.CSS.enable();
  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  // The root node styles should include styles for each pseudo element.
  const rootNodeStyles = await dp.CSS.getMatchedStylesForNode({'nodeId': rootNode.nodeId});
  testRunner.log(rootNodeStyles);

  for (const node of rootNode.pseudoElements) {
    const styles = await dp.CSS.getMatchedStylesForNode({'nodeId': node.nodeId});
    testRunner.log(styles, "Dumping styles for : " + node.localName + " with id " + node.pseudoIdentifier);
  }

  testRunner.completeTest();
});

