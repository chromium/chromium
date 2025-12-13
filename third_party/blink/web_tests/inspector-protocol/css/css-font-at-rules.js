(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
  <style>
  div {
    font-family: Bixa;
    font-palette: --palette;
    font-variant-alternates: swash(fancy);
  }

  @font-palette-values --palette {
    font-family: Bixa;
    override-colors: 0 red;
  }

  @font-feature-values Bixa {
    @swash {
      fancy: 1;
      boring: 2;
    }
    @styleset {
      cool: 3;
    }
    @stylistic {
      something: 4;
    }
  }

  @font-face {
    font-family: Bixa;
    src: local(Bixa);
  }
  </style>

  <div>div</div>
  `,
      'Test that font related at-rules are reported');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});

  const {result: {cssAtRules}} = await dp.CSS.getMatchedStylesForNode({nodeId});

  testRunner.log(cssAtRules);

  testRunner.completeTest();
});
