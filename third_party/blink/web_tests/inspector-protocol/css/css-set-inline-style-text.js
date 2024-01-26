(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-style-text.css')}'/>
<div id='inliner' style='color: red;'>`, 'The test verifies functionality of protocol method CSS.setStyleTexts for inline elements.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#inliner');

  var response = await dp.CSS.getInlineStylesForNode({nodeId});
  var styleSheetId = response.result.inlineStyle.styleSheetId;
  async function setStyleTexts(edits) {
    await cssHelper.setStyleTexts(styleSheetId, false /* expectError */, edits);
    var response = await dp.CSS.getStyleSheetText({styleSheetId});
    testRunner.log('Stylesheet text: ' + response.result.text);
    await dp.DOM.undo();
  };
  async function verifyProtocolError(edits) {
    await cssHelper.setStyleTexts(styleSheetId, true /* expectError */, edits);
    var response = await dp.CSS.getStyleSheetText({styleSheetId});
    testRunner.log('Stylesheet text: ' + response.result.text);
  };

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);
  testRunner.runTestSuite([
    async function testBasicSetStyle() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 11 },
        text: `content: 'EDITED'`,
      }]);
    },

    async function testSetStylePoorContent() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 11 },
        text: '}',
      }]);
    },

    async function testDeleteStyleBody() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 11 },
        text: '',
      }]);
    },

    async function testSetStyleOpenComment() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 11 },
        text: '/*',
      }]);
    }
  ]);
})
