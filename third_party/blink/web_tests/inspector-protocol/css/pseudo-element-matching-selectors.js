(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
#for-pseudo:before {
  color: red;
  content: "BEFORE";
  width: var(--width);
}

#for-pseudo {
  --width: 10px;
}
</style>
<div id='for-pseudo'>Test</div>`, 'Test that matching styles report pseudo element styles.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  var DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');
  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  await cssHelper.requestNodeId(documentNodeId, '#for-pseudo');

  var node = nodeTracker.nodes().find(node => DOMHelper.attributes(node).get('id') === 'for-pseudo');
  var beforeNode = node.pseudoElements[0];

  testRunner.log('\n=== Request matching styles for #for-pseudo::before ===\n');
  var response = await dp.CSS.getMatchedStylesForNode({nodeId: beforeNode.nodeId});
  const inheritedStyle = response.result.inherited;
  for (const {matchedCSSRules} of inheritedStyle) {
    for (const rule of matchedCSSRules) {
      if (rule.rule.style.cssText?.includes('--width: 10px')) {
        testRunner.log('#for-pseudo::before inherits custom properties from parent');
      }
    }
  }

  var matchedRules = response.result.matchedCSSRules;
  for (var i = 0; i < matchedRules.length; ++i) {
      var match = matchedRules[i];
      if (match.rule.selectorList.text === '#for-pseudo::before') {
          testRunner.log('#for-pseudo::before matching the :before element: ' + (match.matchingSelectors[0] === 0));
          testRunner.completeTest();
          return;
      }
  }
  testRunner.log('#for-pseudo::before rule not received');
  testRunner.completeTest();
})
