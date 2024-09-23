(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-position-try-rule-style-text.css')}'/>
<div id='anchor'></div>
<div id='anchored'></div>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts for editing @position-try rules.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;

  const setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);

  const response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  testRunner.runTestSuite([
    async function testBasicSetStyle() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 9, startColumn: 26, endLine: 12, endColumn: 0 },
        text: "\n    bottom: EDITED;\n    right: EDITED\n",
      }]);
    },
 ]);
})
