(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
  }

  @property --len {
    syntax: "<length>";
    initial-value: 4px;
    inherits: false;
  }
  </style>

  <div>div</div>
  <p>p</div>
  `,
      'Test that values of registered properties are not validated against unrelated elements');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});
  const {result: {nodeId: unrelatedNodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'p'});

  const {result: {computedStyle}} =
      await dp.CSS.getComputedStyleForNode({nodeId});
  testRunner.log('Computed value:');
  testRunner.log(computedStyle.find(style => style.name === '--len'));
  testRunner.log(computedStyle.find(style => style.name === '--color'));

  const {result: {matchedCSSRules, cssKeyframesRules}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});

  const rules =
      matchedCSSRules.filter(({rule}) => rule.selectorList.text === 'div');

  const {styleSheetId, range} = rules[1].rule.style;
  testRunner.log('Editing a rule:');
  {
  const edits = [{styleSheetId, range, text: '--v: 5px; --len: var(--v);'}];
    const {result: {styles: [{cssProperties}]}} = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: unrelatedNodeId});
    testRunner.log(cssProperties);
  }

  testRunner.log('Adding a rule:');
  {
    const location = rules[1].rule.selectorList.selectors[0].range;
    location.endColumn = location.startColumn;
    location.endLine = location.startLine;
    const {result: {rule: {style: cssProperties}}} = await dp.CSS.addRule({
      styleSheetId,
      location,
      ruleText: 'div { --v: 5px; --len: var(--v); }',
      nodeForPropertySyntaxValidation: unrelatedNodeId,
    });
    testRunner.log(cssProperties);
  }

  testRunner.completeTest();
});
