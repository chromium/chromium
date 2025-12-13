(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(`
    <style>
      dialog {
        &[open] {
          transition: opacity 0.5s, overlay 0.5s allow-discrete, display 0.5s allow-discrete;
          opacity: 1;
          @starting-style {
            opacity: 0;
          }
        }
        &[open]::backdrop {
          transition: opacity 0.5s;
          opacity: 1;
          @starting-style {
            opacity: 0;
          }
        }
      }
    </style>
    <body>
      <dialog></dialog>
    </body>
  `, 'Test that starting styles with pseudos don\'t hit DCHECK');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function requestDocumentNodeId() {
    const {result} = await dp.DOM.getDocument({});
    return result.root.nodeId;
  }
  async function requestNodeId(nodeId, selector) {
    const {result} = await dp.DOM.querySelector({nodeId, selector});
    return result.nodeId;
  }

  const documentNodeId = await requestDocumentNodeId();

  const pseudoAdded = dp.DOM.oncePseudoElementAdded();

  const nodeId = await requestNodeId(documentNodeId, 'dialog');
  await session.evaluate('document.querySelector("dialog").showModal()');

  await dp.CSS.forceStartingStyle({ nodeId, forced: true });

  const { params: { pseudoElement }} = await pseudoAdded;

  testRunner.log(pseudoElement.localName);

  const nodeIdBackdrop = pseudoElement.nodeId;
  const { result: matchedStylesForNode } = await dp.CSS.getMatchedStylesForNode({ nodeId: nodeIdBackdrop });

  testRunner.log(matchedStylesForNode.matchedCSSRules[3].rule.startingStyles, "starting-styles", [
    'range',
    'styleSheetId'
  ]);

  testRunner.completeTest();
});

