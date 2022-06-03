(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<style>
::marker {
  content: "This should NOT match";
}
#for-pseudo::before {
  content: "BEFORE";
  display: list-item;
}
#for-pseudo::after {
  content: "AFTER";
  display: list-item;
}
#for-pseudo::before::marker {
  content: "MARKER";
}
</style>
<div id='for-pseudo'>Test</div>`, 'Test that matching styles report nested pseudo element styles.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  const nodeTracker = new NodeTracker(dp);
  const DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');
  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  function getPseudoElement(node, ...pseudoTypes) {
    for (const pseudoType of pseudoTypes)
      node = node.pseudoElements.find(pseudoElement => pseudoElement.pseudoType === pseudoType);
    return node;
  }

  async function loadAndDumpMatchingRules(nodeId) {
    const {result} = await dp.CSS.getMatchedStylesForNode({nodeId});
    for (const ruleMatch of result.matchedCSSRules) {
      const origin = ruleMatch.rule.origin;
      if (origin !== 'inspector' && origin !== 'regular')
        continue;
      cssHelper.dumpRuleMatch(ruleMatch);
    }
  }

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  await cssHelper.requestNodeId(documentNodeId, '#for-pseudo');

  const node = nodeTracker.nodes().find(node => DOMHelper.attributes(node).get('id') === 'for-pseudo');

  testRunner.log('\n=== Dump matching styles for #for-pseudo::before::marker ===\n');
  await loadAndDumpMatchingRules(getPseudoElement(node, "before", "marker").nodeId);

  testRunner.log('\n=== Dump matching styles for #for-pseudo::after::marker ===\n');
  await loadAndDumpMatchingRules(getPseudoElement(node, "after", "marker").nodeId);

  testRunner.completeTest();
})
