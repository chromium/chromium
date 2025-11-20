(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
  <style>
  div {
    font-family: Bixa;
    font-variant-alternates: swash(fancy);
  }

  @font-feature-values Bixa {
    @stylistic {
      donttouch: 8;
    }
    @swash {
      fancy: 1;
    }
    @styleset {
      preserve: 9;
    }
  }
  </style>

  <div>div</div>
  `,
      'Test that @font-feature-values sub-rules can be edited');
  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const {result: {cssAtRules}} = await dp.CSS.getMatchedStylesForNode({nodeId});
  const {styleSheetId, range} = cssAtRules[0].style;
  await cssHelper.setStyleTexts(styleSheetId, false, [
    {styleSheetId, range, text: '\n      fancy: 2;\n      boring: 1;\n    '}
  ]);
  testRunner.completeTest();
});
