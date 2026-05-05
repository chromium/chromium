(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
    list-style-type: --my-style;
  }

  @counter-style --my-style {
    system: cyclic;
    symbols: 'a' 'b' 'c';
  }
  </style>

  <div>div</div>
  `,
      'Test that properties of counter-style rules are editable');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const styles = await dp.CSS.getMatchedStylesForNode({nodeId});
  const {result: {cssAtRules: [atRule]}} = styles;
  testRunner.log('At rule:');
  testRunner.log(atRule);
  const {style: {range}, styleSheetId} = atRule;

  const edit = {
    styleSheetId,
    range,
    text: `
    system: fixed;
    symbols: 'x' 'y' 'z';
`,
  };

  testRunner.log('Edit:');
  testRunner.log(edit);

  const result = await dp.CSS.setStyleTexts({edits: [edit]});

  testRunner.log('Edit result:');
  testRunner.log(result);

  testRunner.completeTest();
});
