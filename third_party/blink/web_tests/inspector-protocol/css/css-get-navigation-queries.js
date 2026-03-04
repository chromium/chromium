(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      './resources/navigation-queries.html',
      'Verify that navigation queries are reported properly.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;

  const test = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#test',
  });
  const testId = test.result.nodeId;

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: testId});

  const matchedRules = matchedStyles.result.matchedCSSRules;
  for (const {
         rule: {origin, navigations, ruleTypes, selectorList, style}
       } of matchedRules) {
    if (origin === 'regular') {
      testRunner.log(
          {origin, navigations, ruleTypes, selectorList, style},
          'matched rule: ', [], ['styleSheetId', 'sourceURL']);
    }
  }

  const functionRules = matchedStyles.result.cssFunctionRules;
  for (const functionRule of functionRules) {
    testRunner.log(
        functionRule, 'function rule: ', [], ['styleSheetId', 'sourceURL']);
  }
  testRunner.completeTest();
});
