(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-style-text.css')}'/>
  `, 'Verifies that the range of i-th payload corresponds to the state at the end of i-th edit; not the state at the end of all edits in setStyleTexts operation.');

  dp.DOM.enable();
  dp.CSS.enable();
  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);
  testRunner.runTestSuite([
    async function removeTwoStyleTexts() {
      var edits = [
        {
          styleSheetId: styleSheetId,
          range: { startLine: 4, startColumn: 7, endLine: 10, endColumn: 0 },
          text: '',
        },
        {
            styleSheetId: styleSheetId,
            range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
            text: '',
        },
      ];
      var response = await dp.CSS.setStyleTexts({edits});
      testRunner.log('StylePayload result ranges:');
      for (var i = 0; i < response.result.styles.length; ++i) {
        var stylePayload = response.result.styles[i];
        var range = stylePayload.range;
        var text = `    range #${i}: {${range.startLine}, ${range.startColumn}, ${range.endLine}, ${range.endColumn}}`;
        testRunner.log(text);
      }
    },
  ]);
})
