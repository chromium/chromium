(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-media-text.css')}'/>`, 'Tests CSS.setMediaText method.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;
  var setMediaText = cssHelper.setMediaText.bind(cssHelper, styleSheetId, false);
  var verifyProtocolError = cssHelper.setMediaText.bind(cssHelper, styleSheetId, true);

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  var firstMediaRange = {
    startLine: 0,
    startColumn: 7,
    endLine: 0,
    endColumn: 67
  };
  var secondMediaRange = {
    startLine: 6,
    startColumn: 8,
    endLine: 8,
    endColumn: 23
  };

  testRunner.runTestSuite([
    async function testSimpleEdit() {
      await setMediaText({
        range: firstMediaRange,
        text: 'all and (min-height: 20px)'
      });
      await dp.DOM.undo();
    },

    async function testComplexToSimpleEdit() {
      await setMediaText({
        range: secondMediaRange,
        text: 'all'
      });
      await dp.DOM.undo();
    },

    async function testSimpleToComplexEdit() {
      await setMediaText({
        range: firstMediaRange,
        text: 'all and (min-height: 20px), (max-width: 10px), handheld and (min-monochrome: 8)'
      });
      await dp.DOM.undo();
    },

    async function testInvalidParameters() {
      await verifyProtocolError({
        range: { startLine: 'three', startColumn: 0, endLine: 4, endColumn: 0 },
        text: 'no matter what is here'
      });
    },

    async function testInvalidText() {
      await verifyProtocolError({
        range: firstMediaRange,
        text: 'something /* is wrong here'
      });
    },

    async function testInvalidText2() {
      await verifyProtocolError({
        range: firstMediaRange,
        text: 'something { is { wrong: here} }'
      });
    },

    async function testInvalidText3() {
      await verifyProtocolError({
        range: firstMediaRange,
        text: 'something { wrong'
      });
    },

    async function testEditSequentially() {
      var newText = 'screen';
      var oldLength = firstMediaRange.endColumn - firstMediaRange.startColumn;
      var lengthDelta = newText.length - oldLength;
      await setMediaText({
        range: firstMediaRange,
        text: newText
      });

      var range = {
        startLine: firstMediaRange.startLine,
        startColumn: firstMediaRange.startColumn,
        endLine: firstMediaRange.endLine,
        endColumn: firstMediaRange.endColumn + lengthDelta
      };
      await setMediaText({
        range: range,
        text: 'all,\nhandheld and (min-height: 20px),\n(min-width: 100px) and (max-width: 200px)'
      });
      await dp.DOM.undo();
    },

    async function testSimpleAfterSequentially() {
      await setMediaText({
        range: firstMediaRange,
        text: 'all and (min-height: 20px)'
      });
      await dp.DOM.undo();
    }
  ]);
})
