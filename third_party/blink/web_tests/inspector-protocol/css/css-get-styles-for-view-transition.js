(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <style> ::view-transition-new(shared) {animation-duration: 300s;} </style>
      <style> ::view-transition-old(*) {background: red;} </style>
      <style> ::view-transition-group(.transition-group) {font-size: 10px;} </style>
      <style> ::view-transition-image-pair(shared.transition-group) {left: 100px;} </style>
      <div style='width:100px; height:100px; view-transition-name: shared; view-transition-class: transition-group; contain:paint;'></div>`,
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
  for (const pseudo of rootNodeStyles.result.pseudoElements) {
    testRunner.log(`PseudoElement: ${pseudo.pseudoType}${pseudo.pseudoIdentifier ? ' ' + pseudo.pseudoIdentifier : ''}`);
    for (const match of pseudo.matches) {
      cssHelper.dumpRuleMatch(match);
    }
  }

  for (const node of getAllPseudos(rootNode)) {
    const styles = await dp.CSS.getMatchedStylesForNode({'nodeId': node.nodeId});
    testRunner.log("Dumping styles for : " + node.localName + " with id " + node.pseudoIdentifier);
    for (const match of styles.result.matchedCSSRules) {
      cssHelper.dumpRuleMatch(match);
    }
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
