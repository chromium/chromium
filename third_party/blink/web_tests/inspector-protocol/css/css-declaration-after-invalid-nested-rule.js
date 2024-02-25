(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp} = await testRunner.startHTML(
      `
      <link rel='stylesheet' href='${
          testRunner.url('resources/declaration-after-invalid-nested-rule.css')}'/>
      <div id='target'>
      </div>`,
      'The test verifies functionality of protocol methods working correctly with CSS declarations that appear after invalid nested rules');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;
  const targetNode = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#target',
  });
  const targetNodeId = targetNode.result.nodeId;

  const matchedStyles =
      await dp.CSS.getMatchedStylesForNode({nodeId: targetNodeId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    cssHelper.dumpRuleMatch(ruleMatch);
  }

  testRunner.completeTest();
});
