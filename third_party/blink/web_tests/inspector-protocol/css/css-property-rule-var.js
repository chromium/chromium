(async function(testRunner) {
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
  </style>

  <div>div</div>
  `,
      'Test that values of property rules are computed correctly in the presence of var()');

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

  const {result: {matchedCSSRules, cssKeyframesRules}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});

  testRunner.log('Validated declarations:');
  testRunner.log(
      matchedCSSRules.filter(({rule}) => rule.selectorList.text === 'div')
          .map(({rule}) => rule.style.cssProperties)
          .flat()
          .filter(({name}) => name.startsWith('--'))
          .flat());
  testRunner.log('Keyframes:');
  testRunner.log(cssKeyframesRules);

  testRunner.completeTest();
});
