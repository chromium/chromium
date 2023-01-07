(async function (testRunner) {
  const { dp } = await testRunner.startURL(
    '../resources/css-container-queries-in-constructed-stylesheet.html',
    'Test CSS.getMatchedStylesForNode and CSS.setContainerQueryText methods for container queries in a constructed stylesheet');

  const CSSHelper = await testRunner.loadScript('../../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const itemNodeId = await cssHelper.requestNodeId(documentNodeId, '.item');

  const { result } = await dp.CSS.getMatchedStylesForNode({ nodeId: itemNodeId });
  const matchedRule = result.matchedCSSRules.find(match => match.rule.selectorList.text === '.item');
  if (!matchedRule) {
    testRunner.log('container query experiment not enabled');
    testRunner.completeTest();;
  }
  cssHelper.dumpRuleMatch(matchedRule);

  const styleSheetId = matchedRule.rule.styleSheetId;

  await cssHelper.setContainerQueryText(styleSheetId, false, {
    range: {
      startColumn: 11,
      startLine: 1,
      endColumn: 29,
      endLine: 1,
    },
    text: '(max-width: 300px)',
  });

  await cssHelper.loadAndDumpMatchingRulesForNode(itemNodeId);

  testRunner.completeTest();
});
