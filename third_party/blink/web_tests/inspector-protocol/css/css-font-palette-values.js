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
      'Test that font-palette-values rules are reported');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const {result: {cssFontPaletteValuesRule}} = await dp.CSS.getMatchedStylesForNode({nodeId});

  testRunner.log(cssFontPaletteValuesRule);

  testRunner.completeTest();
});
