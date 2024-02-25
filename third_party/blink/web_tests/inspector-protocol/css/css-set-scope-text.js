(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-scope-text.css')}'/>`, 'Tests CSS.setScopeText method.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;
  const setScopeText = cssHelper.setScopeText.bind(cssHelper, styleSheetId, false);
  const verifyProtocolError = cssHelper.setScopeText.bind(cssHelper, styleSheetId, true);

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  const scopeRange = {
    startLine: 0,
    startColumn: 7,
    endLine: 0,
    endColumn: 38,
  };

  testRunner.runTestSuite([
    async function testSimpleEdit() {
      await setScopeText({
        range: scopeRange,
        text: '(body)',
      });
      await dp.DOM.undo();
    },

    async function testInvalidParameters() {
      await verifyProtocolError({
        range: { startLine: 'three', startColumn: 0, endLine: 4, endColumn: 0 },
        text: 'no matter what is here',
      });
    },

    async function testInvalidText() {
      await verifyProtocolError({
        range: scopeRange,
        text: 'something { is { wrong: here} }'
      });
    },

    async function testEditSequentially() {
      const newText = '(body)';
      const oldLength = scopeRange.endColumn - scopeRange.startColumn;
      const lengthDelta = newText.length - oldLength;
      await setScopeText({
        range: scopeRange,
        text: newText,
      });

      const newRange = {
        ...scopeRange,
        endColumn: scopeRange.endColumn + lengthDelta,
      };
      await setScopeText({
        range: newRange,
        text: '(.dark-theme)'
      });
      await dp.DOM.undo();
    },

    async function testAfterSequentially() {
      await setScopeText({
        range: scopeRange,
        text: '(.light-theme) to (.dark-theme)'
      });
      await dp.DOM.undo();
    },
  ]);
})
