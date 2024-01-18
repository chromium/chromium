(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, page} = await testRunner.startBlank(
      'Test CSS.getMatchedStylesForNode and CSS.setContainerQueryText methods for container queries in a constructed stylesheet');

  const CSSHelper = await testRunner.loadScript('../../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  await page.navigate(
      '../resources/css-container-queries-in-constructed-stylesheet.html');

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const itemNodeId = await cssHelper.requestNodeId(documentNodeId, '.item');

  const { result } = await dp.CSS.getMatchedStylesForNode({ nodeId: itemNodeId });
  const matchedRule = result.matchedCSSRules.find(match => match.rule.selectorList.text === '.item');
  if (!matchedRule) {
    testRunner.log('container query experiment not enabled');
    testRunner.completeTest();
  }
  cssHelper.dumpRuleMatch(matchedRule);

  const styleSheetId = matchedRule.rule.styleSheetId;

  await cssHelper.setContainerQueryText(styleSheetId, false, {
    range: {
      startColumn: 13,
      startLine: 6,
      endColumn: 31,
      endLine: 6,
    },
    text: '(max-width: 300px)',
  });

  await cssHelper.loadAndDumpMatchingRulesForNode(itemNodeId);

  testRunner.completeTest();
});
