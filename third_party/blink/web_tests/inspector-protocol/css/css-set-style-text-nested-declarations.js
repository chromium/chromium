(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-style-text-nested-declarations.css')}'/>
<article id='test'></article>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts modifying CSSNestedDeclarations.');

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
    async function testBasicEdit() {
      // Original text: "width: 100px;\n  height: 50px;"
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine:4, startColumn: 2, endLine: 5, endColumn: 15 },
        text: "width: 142px;\n  height: 92px;",
      }]);
      await dumpAndUndo();
    },
    // Tests editing of the trailing CSSNestedDeclarations rule.
    async function testEditTrailing() {
      // Original text: "top: 3px"
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine:7, startColumn: 2, endLine: 7, endColumn: 11 },
        text: "top: 45px",
      }]);
      await dumpAndUndo();
    },
    async function testEditComment() {
      // Original text: "width: 100px;\n  height: 50px;"
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine:4, startColumn: 2, endLine: 5, endColumn: 15 },
        text: "/* width: 142px; */\n  height: 92px;",
      }]);
      await dumpAndUndo();
    },

    // TODO(crbug.com/361116768): We do not support making edits
    // which would remove the CSSNestedDeclarations rule.
    async function testEditCommentAllDeclarations() {
      // Original text: "width: 100px;\n  height: 50px;"
      await verifyProtocolError([{
        styleSheetId: styleSheetId,
        range: { startLine:4, startColumn: 2, endLine: 5, endColumn: 15 },
        text: "/* width: 142px; */\n  /* height: 92px; */",
      }]);
    },
 ]);
})
