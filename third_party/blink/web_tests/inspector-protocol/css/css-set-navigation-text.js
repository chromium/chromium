(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      './resources/navigation-queries.html',
      'Tests CSS.setNavigationText method.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;
  const setNavigationText =
      cssHelper.setNavigationText.bind(cssHelper, styleSheetId, false);
  const verifyProtocolError =
      cssHelper.setNavigationText.bind(cssHelper, styleSheetId, true);

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  const navigationRange = {
    startLine: 29,
    startColumn: 18,
    endLine: 29,
    endColumn: 35,
  };

  testRunner.runTestSuite([
    async function testSimpleEdit() {
      await setNavigationText({
        range: navigationRange,
        text: '(at: --not-this-page)',
      });
      await dp.DOM.undo();
    },

    async function testInvalidParameters() {
      await verifyProtocolError({
        range: {startLine: 'three', startColumn: 0, endLine: 4, endColumn: 0},
        text: 'no matter what is here',
      });
    },

    async function testInvalidText() {
      await verifyProtocolError(
          {range: navigationRange, text: 'something { is { wrong: here} }'});
    },

    async function testEditSequentially() {
      const newText = '(at: --not-this-page)';
      const oldLength = navigationRange.endColumn - navigationRange.startColumn;
      const lengthDelta = newText.length - oldLength;
      await setNavigationText({
        range: navigationRange,
        text: newText,
      });

      const newRange = {
        ...navigationRange,
        endColumn: navigationRange.endColumn + lengthDelta,
      };
      await setNavigationText(
          {range: newRange, text: 'not (at: --not-this-page)'});
      await dp.DOM.undo();
    },

    async function testAfterSequentially() {
      await setNavigationText(
          {range: navigationRange, text: 'not (at: --this-page)'});
      await dp.DOM.undo();
    },
  ]);
})
