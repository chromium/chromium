(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
    color: var(--prop);
    background-color: var(--second-prop);
  }

  @property --prop {
    syntax: "<color>";
    inherits: false;
    initial-value: red;
  }
  </style>

  <div>div</div>
  <p>p</p>
  `,
      'Test that properties of property rules are editable');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});
  {
    const {result: {computedStyle}} =
        await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log('Original color:');
    testRunner.log(computedStyle.find(style => style.name === 'color'));
  }

  const {result: {cssPropertyRules: [{style: {range}, styleSheetId}]}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});

  const edit = {
    styleSheetId,
    range,
    text: `
    syntax: "<color>";
    inherits: false;
    initial-value: blue;
`,
  };

  testRunner.log('Edit:');
  testRunner.log(edit);

  const result = await dp.CSS.setStyleTexts({edits: [edit]});

  testRunner.log('Edit result:');
  testRunner.log(result);

  {
    const {result: {computedStyle}} =
        await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log('Modified color:');
    testRunner.log(computedStyle.find(style => style.name === 'color'));
  }

  testRunner.completeTest();
});
