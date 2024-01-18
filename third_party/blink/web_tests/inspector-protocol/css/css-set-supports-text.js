(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-supports-text.css')}'/>`, 'Tests CSS.setSupportsText method.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;
  const setSupportsText = cssHelper.setSupportsText.bind(cssHelper, styleSheetId, false);
  const verifyProtocolError = cssHelper.setSupportsText.bind(cssHelper, styleSheetId, true);

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  const supportsRange = {
    startLine: 0,
    startColumn: 10,
    endLine: 0,
    endColumn: 25,
  };

  testRunner.runTestSuite([
    async function testSimpleEdit() {
      await setSupportsText({
        range: supportsRange,
        text: '(display: flex)',
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
        range: supportsRange,
        text: 'something { is { wrong: here} }'
      });
    },

    async function testEditSequentially() {
      const newText = '(display: flex)';
      const oldLength = supportsRange.endColumn - supportsRange.startColumn;
      const lengthDelta = newText.length - oldLength;
      await setSupportsText({
        range: supportsRange,
        text: newText,
      });

      const newRange = {
        ...supportsRange,
        endColumn: supportsRange.endColumn + lengthDelta,
      };
      await setSupportsText({
        range: newRange,
        text: '(display: none)'
      });
      await dp.DOM.undo();
    },

    async function testAfterSequentially() {
      await setSupportsText({
        range: supportsRange,
        text: '(display: inline)'
      });
      await dp.DOM.undo();
    },
  ]);
})
