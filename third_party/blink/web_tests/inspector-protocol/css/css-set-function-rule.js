(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
    color: --get-color();
  }

  @function --get-color() {
    --col: gray;
    @supports (color: rod) {
      --col: rod;
    }
    result: var(--cal);
  }
  </style>

  <div>div</div>
  `,
      'Test that properties of function rules are editable');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const styles = await dp.CSS.getMatchedStylesForNode({nodeId});
  const {result: {cssFunctionRules: [functionRule]}} = styles;
  testRunner.log('Function:');
  testRunner.log(functionRule);
  const {
    children: [
      {style: {range: firstRange}}, {
        condition: {
          supports: {range: queryRange},
          children: [{style: {range: nestedRange}}],
        }
      },
      {style: {range: lastRange}}
    ],
    styleSheetId
  } = functionRule;

  async function editProperties(range, text) {
    const edit = {
      styleSheetId,
      range,
      text,
    };

    const result = await dp.CSS.setStyleTexts({edits: [edit]});

    testRunner.log('Edit result:');
    testRunner.log(result);
  }

  // Edit from the top down.
  await editProperties(firstRange, '--col: grey;');

  // Edit the condition.
  const editCondition = {
    styleSheetId,
    range: queryRange,
    text: '(color: red)',
  };

  const result = await dp.CSS.setSupportsText(editCondition);

  testRunner.log('Edit result:');
  testRunner.log(result);

  // Edit the nested rule.
  await editProperties(nestedRange, '--col: red;');
  // Edit the result.
  await editProperties(lastRange, 'result: var(--col);');

  testRunner.log('Final style sheet text:');
  const {result: {text}} = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log(text);

  testRunner.completeTest();
});
