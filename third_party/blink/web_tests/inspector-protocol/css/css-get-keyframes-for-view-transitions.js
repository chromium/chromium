(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startHTML(`
        <style>
            @keyframes foo { from { opacity: 0.5; } }
            ::view-transition-old(*) { animation-name: foo; animation-duration: 30s; }
        </style>
        <div style='width:100px; height:100px; view-transition-name: shared; contain:paint;'></div>`,
        'The test verifies functionality of querying keyframe information for view-transition pseudo elements');

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

    for (const node of getAllPseudos(rootNode)) {
      testRunner.log("Dumping animations for : " + node.localName + " with id " + node.pseudoIdentifier);
      await cssHelper.loadAndDumpCSSAnimationsForNode(node.nodeId);
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
