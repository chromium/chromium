(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
    --in: 2px;
    --len: calc(var(--in) * 4);
    --color-in: purple;
    --color: var(--in);
    width: var(--len);
    animation: --animation 0s linear;
  }

  @property --len {
    syntax: "<length>";
    initial-value: 4px;
    inherits: false;
  }
  @property --color {
    syntax: "<color>";
    initial-value: red;
    inherits: false;
  }
  @keyframes --animation {
    from {
      --color: var(--color-in);
    }
    to {
      --color: blue;
    }
  }
  body::before {
    --m: 0;
    --len: var(--m);
    counter-reset: n var(--len);
    content: counter(n);
  }
  div::before {
    --m: 0;
    --len: var(--m);
  }
  </style>

  <div>div</div>
  `,
      'Test that values of registered properties are validated correctly in the presence of var()');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();

  const nodes = new Map();
  function collectNodes(node) {
    nodes.set(node.localName, node.nodeId);
    node.children?.forEach(collectNodes);
    node.pseudoElements?.forEach(collectNodes);
  }
  collectNodes(root);

  const {result: {computedStyle}} =
      await dp.CSS.getComputedStyleForNode({nodeId: nodes.get('div')});
  testRunner.log('Computed value:');
  testRunner.log(computedStyle.find(style => style.name === '--len'));
  testRunner.log(computedStyle.find(style => style.name === '--color'));

  const {result: {matchedCSSRules, cssKeyframesRules, pseudoElements}} =
      await dp.CSS.getMatchedStylesForNode({nodeId: nodes.get('div')});

  const rules =
      matchedCSSRules.filter(({rule}) => rule.selectorList.text === 'div');
  testRunner.log('Validated declarations:');
  testRunner.log(rules.map(({rule}) => rule.style.cssProperties)
                     .flat()
                     .filter(({name}) => name.startsWith('--'))
                     .flat());
  testRunner.log('Pseudo Elements:');
  testRunner.log(
      pseudoElements
          .map(
              ({matches}) =>
                  matches.map(({rule}) => rule.style.cssProperties).flat())
          .flat()
          .filter(({name}) => name.startsWith('--')));
  testRunner.log('Keyframes:');
  testRunner.log(cssKeyframesRules);

  const {styleSheetId, range} = rules[1].rule.style;
  testRunner.log('Editing a rule:');
  {
    const edits = [{styleSheetId, range, text: '--v: 5px; --len: var(--v);'}];
    const {result: {styles: [{cssProperties}]}} = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
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
      nodeForPropertySyntaxValidation: nodes.get('div'),
    });
    testRunner.log(cssProperties);
  }

  testRunner.log('Pseudo Elements:');
  {
    const {result: {pseudoElements}} =
        await dp.CSS.getMatchedStylesForNode({nodeId: nodes.get('body')});
    testRunner.log(
        pseudoElements
            .map(
                ({matches}) =>
                    matches.map(({rule}) => rule.style.cssProperties).flat())
            .flat()
            .filter(({name}) => name.startsWith('--')));
    const {result: {matchedCSSRules, cssPropertyRules}} =
        await dp.CSS.getMatchedStylesForNode({nodeId: nodes.get('::before')});
    testRunner.log(
        matchedCSSRules
            .filter(({rule}) => rule.selectorList.text === 'body::before')
            .map(({rule}) => rule.style.cssProperties)
            .flat()
            .filter(({name}) => name.startsWith('--'))
            .flat());
    testRunner.log('Pseudo element registered properties:');
    testRunner.log(cssPropertyRules.map(({propertyName}) => propertyName));
  }

  testRunner.completeTest();
});
