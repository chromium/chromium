(async function(testRunner) {
  var {dp} = await testRunner.startHTML(`
      <link rel='stylesheet' href='${testRunner.url('resources/nested-styles.css')}'/>
      <div id='parent'>
          <div id='nested'></div>
      </div>`,
      'The test verifies functionality of protocol methods working correctly with CSS nesting.');

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

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: nestedNodeId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
  }

  const styleSheetId = matchedStyles.result.matchedCSSRules.at(-1).rule.styleSheetId;
  await cssHelper.setStyleTexts(styleSheetId, false, [{
    styleSheetId,
    range: { startLine: 1, startColumn: 11, endLine: 3, endColumn: 2 },
    text: "\n    width: 41px;\n  ",
  }]);

  const updatedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: nestedNodeId});
  for (const ruleMatch of updatedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
  }

  testRunner.completeTest();
});
