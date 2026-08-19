(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
<style>
@font-feature-values serif {
  @styleset {
    preserve: 9;
  }
}

body {
  color: green;
}
</style>
<body></body>
`,
      'Verify that an @font-feature-values rule with an invalid generic family ' +
          'name does not corrupt the source data of the rules following it.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;
  const body = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: 'body',
  });
  const bodyId = body.result.nodeId;

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: bodyId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    const rule = ruleMatch.rule;
    if (rule.origin !== 'regular') {
      continue;
    }
    testRunner.log(`selector: ${rule.selectorList.text}`);
    testRunner.log(`has source range: ${Boolean(rule.style.range)}`);
  }

  testRunner.completeTest();
});
