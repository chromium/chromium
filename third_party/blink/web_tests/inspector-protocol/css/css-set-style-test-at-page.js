(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/at-page.css')}'/>
<article id='test'></article>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts for @page.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;

  const setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);
  const verifyProtocolError = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, true);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  const response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  async function dumpAndUndo() {
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');
    await dp.DOM.undo();
  }

  testRunner.runTestSuite([
    async function testCanChangeAtPage() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 4, startColumn: 7, endLine: 7, endColumn: 0 },
        text: "\n  size: 5in 5in;\n",
      }]);
      await dumpAndUndo();
    },

    async function testCanChangeRulesBefore() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 9, endLine: 2, endColumn: 0 },
        text: "\n  color: green;\n",
      }]);
      await dumpAndUndo();
    },

    async function testCanChangeRulesAfter() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 9, startColumn: 7, endLine: 11, endColumn: 0 },
        text: "\n  background-color: blue;\n",
      }]);
      await dumpAndUndo();
    },
 ]);
})
