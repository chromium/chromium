(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-style-text.css')}'/>
<article id='test'></article>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;

  var setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);
  var verifyProtocolError = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, true);

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  async function dumpAndUndo() {
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');
    await dp.DOM.undo();
  }

  testRunner.runTestSuite([
    async function testBasicSetStyle() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: "\n    content: 'EDITED';\n",
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleTwice() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: '\n    color: green;\n    padding: 0 4px;\n    cursor: pointer\n',
      }]);
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');

      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 4, endColumn: 0 },
        text: '\n    color: green;\n    padding: 0 6px;\n    cursor: pointer\n',
      }]);
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');

      await setStyleTexts([{
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 4, endColumn: 0 },
          text: '\n    color: green;\n    padding: 0 8px;\n    cursor: pointer\n',
      }]);
      await dumpAndUndo();
    },

    async function testSetStylePoorContent() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: '}',
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleOpenBrace() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: '{',
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleComment() {
      await verifyProtocolError([{
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: '/*',
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleInMedia() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
        text: "\n        content: 'EDITED';\n        color: red;\n        /** foo */\n    ",
      }]);
      await dumpAndUndo();
    },

    async function testDeleteStyleBody() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
        text: '',
      }]);
      await dumpAndUndo();
    },

    async function testSetStylePoorRange() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 11, startColumn: 11, endLine: 15, endColumn: 4 },
        text: "\n        content: 'EDITED';\n",
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleOpenComment() {
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
        text: "\n        content: 'EDITED'/* ;\n",
      }]);
      await dumpAndUndo();
    },

    async function testSetStyleOfRemovedRule() {
      await session.evaluate(() => document.styleSheets[0].removeRule(0));

      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: "\n    content: 'EDITED';\n",
      }]);
    },
 ]);
})
