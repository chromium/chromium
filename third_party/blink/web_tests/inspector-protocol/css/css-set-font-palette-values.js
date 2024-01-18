(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
       `
  <style>
  div {
    font-family: Bixa;
    font-palette: --palette;
  }

  @font-palette-values --palette {
    font-family: Bixa;
    override-colors: 0 red;
  }
  </style>

  <div>div</div>
  `,
      'Test that properties of font-palette-value rules are editable');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const styles = await dp.CSS.getMatchedStylesForNode({nodeId});
  const {result: {cssFontPaletteValuesRule: {style: {range}, styleSheetId}}} = styles;

  const edit = {
    styleSheetId,
    range,
    text: `
    font-family: Bixa;
    override-colors: 0 purple;
`,
  };

  testRunner.log('Edit:');
  testRunner.log(edit);

  const result = await dp.CSS.setStyleTexts({edits: [edit]});

  testRunner.log('Edit result:');
  testRunner.log(result);

  testRunner.completeTest();
});
