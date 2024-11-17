(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      'Tests that constructed stylesheets can be edited');

  let CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  let cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  await page.loadHTML(`
    <script>
      function setSheet(text) {
        let sheet = new CSSStyleSheet();
        sheet.replaceSync(text);
        document.adoptedStyleSheets = [sheet];
      }
    </script>
  `);

  await session.evaluate(
    `setSheet('#test { left: 100px; &{} }')`);
  let event = await dp.CSS.onceStyleSheetAdded();
  let styleSheetId = event.params.header.styleSheetId;

  let setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  // Don't crash, crbug.com/376354507.
  await setStyleTexts([{
    styleSheetId: styleSheetId,
    range: { startLine: 0, startColumn: 7, endLine: 0, endColumn: 20 },
    text: " right: 100px;",
  }]);

  testRunner.completeTest();
});
