(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    `<html>
      <style>
        div.standard_font {
        }
        div.fixed_font {
          font-family: monospace;
        }
      </style>
      <body>
        <div class=standard_font>Standard</div>
        <div class=fixed_font>Fixed</div>
      </body>
    </html>`,
  'Tests Page.setFontSizes.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function logFontSizes(selector) {
    testRunner.log(selector);
    const root = (await dp.DOM.getDocument()).result.root;
    const nodeId = (await dp.DOM.querySelector(
      {nodeId: root.nodeId, selector: selector})).result.nodeId;
    const computedStyle = (await dp.CSS.getComputedStyleForNode(
      {nodeId: nodeId})).result.computedStyle;
    testRunner.log(computedStyle.filter(pair => pair.name == 'font-size'));
  }

  // Override default font sizes.
  await dp.Page.setFontSizes({fontSizes: {
    standard: 32,
    fixed: 48
  }});

  // Log overriden fonts
  await logFontSizes('.standard_font');
  await logFontSizes('.fixed_font');

  testRunner.completeTest();
})
