(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
  <style>
  #test {
    font-family: Papyrus;
  }

  #test::before {
    content: "before";
    font-family: Bixa;
    font-palette: --palette;
    font-variant-alternates: swash(fancy);
  }

  #test::after {
    content: "after";
    font-family: Bixa;
    font-palette: --palette;
    font-variant-alternates: swash(boring);
  }

  #test::first-letter {
    font-family: Bixa;
    font-variant-alternates: styleset(cool);
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

  <div id=test>div</div>
  `,
      'Test that font related at-rules are reported when used in pseudo-elements');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: '#test'});

  const {result: {cssAtRules}} = await dp.CSS.getMatchedStylesForNode({nodeId});

  testRunner.log(cssAtRules);

  testRunner.completeTest();
});
