(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-try-rule-style-text.css')}'/>
<div id='anchor'></div>
<div id='anchored'></div>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts for editing @try rules.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;

  var setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  testRunner.runTestSuite([
    async function testBasicSetStyle() {
      await setStyleTexts([{
        styleSheetId: styleSheetId,
        range: { startLine: 10, startColumn: 10, endLine: 13, endColumn: 4 },
        text: "\n        bottom: EDITED;\n        right: EDITED\n    ",
      }]);
    },
 ]);
})
