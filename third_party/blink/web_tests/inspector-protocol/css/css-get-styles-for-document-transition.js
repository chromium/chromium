(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
      <style> ::page-transition-incoming-image(shared) {animation-duration: 300s;} </style>
      <style> ::page-transition-outgoing-image(*) {background: red;} </style>
      <div style='width:100px; height:100px; page-transition-tag: shared; contain:paint;'></div>`,
      'The test verifies functionality of querying style information for document-transition pseudo elements');

  await dp.DOM.enable();
  await dp.CSS.enable();

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await session.evaluateAsync(`
     let transitionStartedCallback;
     let t = document.createDocumentTransition();
     t.start(() => { transitionStartedCallback(); });
     new Promise(resolve => {
       transitionStartedCallback = resolve;
     });
  `);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const rootNodeId = await cssHelper.requestNodeId(documentNodeId, 'html');

  // The root node styles should include styles for each pseudo element.
  result = await dp.CSS.getMatchedStylesForNode({'nodeId': rootNodeId});
  testRunner.log(result);

  var {result} = await dp.DOM.getFlattenedDocument();
  var rootNode = result.nodes.filter(node => node.pseudoElements)[0];

  for (node of rootNode.pseudoElements) {
    testRunner.log("Dumping styles for : " + node.localName);
    let styles = await dp.CSS.getMatchedStylesForNode({'nodeId': node.nodeId});
    testRunner.log(styles);
  }

  testRunner.completeTest();
});

