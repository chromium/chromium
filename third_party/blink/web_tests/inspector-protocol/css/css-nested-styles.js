(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp} = await testRunner.startHTML(
      `
      <link rel='stylesheet' href='${
          testRunner.url('resources/nested-styles.css')}'/>
      <div id='parent'>
          <div id='nested'>
            <div id='deep-nested'></div>
          </div>
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

  const matchedStyles =
      await dp.CSS.getMatchedStylesForNode({nodeId: nestedNodeId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
    if (ruleMatch.rule.nestingSelectors) {
      // NOTE: The output selectors are separated by comma, but this is _not_
      // a list of complex selectors; the comma separates the nested and parent
      // selectors in the devtools protocol.
      testRunner.log('nesting selectors: ' + ruleMatch.rule.nestingSelectors);
    }
  }

  const deepNestedNode = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#deep-nested',
  });
  const deepNestedNodeId = deepNestedNode.result.nodeId;
  const deepNestedMatchedStyles =
      await dp.CSS.getMatchedStylesForNode({nodeId: deepNestedNodeId});
  for (const ruleMatch of deepNestedMatchedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
    if (ruleMatch.rule.nestingSelectors) {
      testRunner.log('nesting selectors: ' + ruleMatch.rule.nestingSelectors);
    }
  }

  const styleSheetId =
      matchedStyles.result.matchedCSSRules.at(-1).rule.styleSheetId;
  await cssHelper.setStyleTexts(
      styleSheetId, false, [{
        styleSheetId,
        range: {startLine: 1, startColumn: 11, endLine: 2, endColumn: 16},
        text: '\n    width: 41px;\n    ',
      }]);

  const updatedStyles =
      await dp.CSS.getMatchedStylesForNode({nodeId: nestedNodeId});
  for (const ruleMatch of updatedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
  }

  testRunner.completeTest();
});
