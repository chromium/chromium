(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <style> ::view-transition-new(shared) {animation-duration: 300s;} </style>
      <style> ::view-transition-old(*) {background: red;} </style>
      <div style='width:100px; height:100px; view-transition-name: shared; contain:paint;'></div>`,
      'The test verifies functionality of querying style information for view-transition pseudo elements');

  await session.evaluateAsync(`
     new Promise( async (resolve) => {
       // Wait for the promise below and query style to ensure all
       // pseudo-elements are generated before using the devtools API.
       await document.startViewTransition().ready;
       window.getComputedStyle(document.documentElement, "::view-transition-new(root)").background;
       resolve();
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

  for (const node of getAllPseudos(rootNode)) {
    const styles = await dp.CSS.getMatchedStylesForNode({'nodeId': node.nodeId});
    testRunner.log(styles, "Dumping styles for : " + node.localName + " with id " + node.pseudoIdentifier);
  }

  testRunner.completeTest();

  function getAllPseudos(rootNode) {
    let pseudos = [];

    if (rootNode.pseudoElements === undefined) {
      return pseudos;
    }

    for (const node of rootNode.pseudoElements) {
      pseudos = pseudos.concat(node);
      pseudos = pseudos.concat(getAllPseudos(node));
    }

    return pseudos;
  };
});
