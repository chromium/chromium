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
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const {result: {computedStyle}} =
      await dp.CSS.getComputedStyleForNode({nodeId});
  testRunner.log('Computed value:');
  testRunner.log(computedStyle.find(style => style.name === '--len'));
  testRunner.log(computedStyle.find(style => style.name === '--color'));

  const {result: {matchedCSSRules, cssKeyframesRules, pseudoElements}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});

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
        {edits, nodeForPropertySyntaxValidation: nodeId});
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
      nodeForPropertySyntaxValidation: nodeId,
    });
    testRunner.log(cssProperties);
  }

  testRunner.log('Pseudo Elements:');
  {
    const {result: {nodeId}} =
        await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'body'});
    const {result: {pseudoElements}} =
        await dp.CSS.getMatchedStylesForNode({nodeId});
    testRunner.log(
        pseudoElements
            .map(
                ({matches}) =>
                    matches.map(({rule}) => rule.style.cssProperties).flat())
            .flat()
            .filter(({name}) => name.startsWith('--')));
  }


  testRunner.completeTest();
});
