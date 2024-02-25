(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' type='text/css' href='${testRunner.url('resources/keyframes.css')}'></link>
<style>
#element {
    animation: animName 1s 2s, mediaAnim 2s, doesNotExist 3s, styleSheetAnim 0s;
}

#element::before {
    content: 'not-empty';
    animation: beforeAnim 1s;
}

@keyframes animName {
    from {
        width: 100px;
    }
    10% {
        width: 150px;
    }
    100% {
        width: 200px;
    }
}

@media (min-width: 1px) {
    @keyframes mediaAnim {
        from {
            opacity: 0;
        }
        to {
            opacity: 1;
        }
    }
}

@keyframes beforeAnim {
    from {
        width: 10px;
    }

    to {
        width: 50px;
    }
}

</style>
<div id='element'></div>
`, 'Test that keyframe rules are reported.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  const nodeTracker = new NodeTracker(dp);
  const DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');

  function getPseudoElement(node, ...pseudoTypes) {
    for (const pseudoType of pseudoTypes)
      node = node.pseudoElements.find(pseudoElement => pseudoElement.pseudoType === pseudoType);
    return node;
  }

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var elementNodeId = await cssHelper.requestNodeId(documentNodeId, '#element');
  testRunner.log("\nAnimations for #element");
  await cssHelper.loadAndDumpCSSAnimationsForNode(elementNodeId);


  const node = nodeTracker.nodes().find(node => DOMHelper.attributes(node).get('id') === 'element');
  testRunner.log("\nAnimations for #element::before");
  const beforeNodeId = getPseudoElement(node, "before").nodeId;
  await cssHelper.loadAndDumpCSSAnimationsForNode(beforeNodeId);
  testRunner.completeTest();
});
