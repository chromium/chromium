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

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  testRunner.runTestSuite([
    async function testMalformedArguments1() {
      await verifyProtocolError([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED';\n",
        },
        {
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED';\n",
        },
      ]);
    },

    async function testMalformedArguments2() {
      await verifyProtocolError([
        {
          styleSheetId: styleSheetId,
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED';\n",
        },
        {
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED';\n",
        },
      ]);
    },

    async function testMalformedArguments3() {
      await verifyProtocolError([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 'STRING INSTEAD OF NUMBER', startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED';\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED';\n",
        },
      ]);
    },

    async function testFirstEditDoesNotApply() {
      await verifyProtocolError([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED';/*\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED';\n",
        },
      ]);
    },

    async function testSecondEditDoesNotApply() {
      await verifyProtocolError([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED';\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED';/*\n",
        },
      ]);
    },

    async function testBasicSetStyle() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: "\n    content: 'EDITED';\n"
      }]);
      await dp.DOM.undo();
    },

    async function testMultipleStyleTexts1() {
      await setStyleTexts([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED1';\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED2';\n",
        },
      ]);
      await dp.DOM.undo();
    },

    async function testMultipleStyleTexts2() {
      await setStyleTexts([
        {
          styleSheetId: styleSheetId,
          range: { startLine: 17, startColumn: 11, endLine: 18, endColumn: 4 },
          text: "\n        content: 'EDITED5';\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 13, startColumn: 11, endLine: 15, endColumn: 4 },
          text: "\n        content: 'EDITED4';\n",
        },
        {
          styleSheetId: styleSheetId,
          range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
          text: "\n    content: 'EDITED3';\n",
        },
      ]);
      await dp.DOM.undo();
    },
 ]);
})

