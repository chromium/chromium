(async function(testRunner) {
  var {dp} = await testRunner.startHTML(
      `
      <link rel='stylesheet' href='${
          testRunner.url('resources/nested-ident-styles.css')}'/>
      <div id='parent'>
          <div>
            <div id='nested'></div>
          </div>
      </div>`,
      'The test verifies functionality of protocol methods working correctly with CSS nesting rules starting with identifiers');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;
  const nestedNode = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#nested',
  });
  const nestedNodeId = nestedNode.result.nodeId;

  const matchedStyles =
      await dp.CSS.getMatchedStylesForNode({nodeId: nestedNodeId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
    if (ruleMatch.rule.nestingSelectors) {
      testRunner.log('nesting selectors: ' + ruleMatch.rule.nestingSelectors);
    }
  }

  testRunner.completeTest();
});
