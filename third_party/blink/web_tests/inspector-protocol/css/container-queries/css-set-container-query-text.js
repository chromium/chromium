(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('../resources/set-container-query-text.css')}'/>`, 'Tests CSS.setContainerQueryText method.');

  const CSSHelper = await testRunner.loadScript('../../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  const event = await dp.CSS.onceStyleSheetAdded();
  const styleSheetId = event.params.header.styleSheetId;
  const setContainerQueryText = cssHelper.setContainerQueryText.bind(cssHelper, styleSheetId, false);
  const verifyProtocolError = cssHelper.setContainerQueryText.bind(cssHelper, styleSheetId, true);

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  const containerQueryRange = {
    startLine: 0,
    startColumn: 11,
    endLine: 0,
    endColumn: 55,
  };

  testRunner.runTestSuite([
    async function testSimpleEdit() {
      await setContainerQueryText({
        range: containerQueryRange,
        text: '((min-width: 100px) and (max-height: 200px))',
      });
      await dp.DOM.undo();
    },

    async function testFeatureChange() {
      await setContainerQueryText({
        range: containerQueryRange,
        text: '(min-aspect-ratio: 1 / 1000)',
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
        range: containerQueryRange,
        text: 'something { is { wrong: here} }'
      });
    },

    async function testEditSequentially() {
      const newText = '(min-width: 50px)';
      const oldLength = containerQueryRange.endColumn - containerQueryRange.startColumn;
      const lengthDelta = newText.length - oldLength;
      await setContainerQueryText({
        range: containerQueryRange,
        text: newText,
      });

      const newRange = {
        ...containerQueryRange,
        endColumn: containerQueryRange.endColumn + lengthDelta,
      };
      await setContainerQueryText({
        range: newRange,
        text: '(min-height: 80px)'
      });
      await dp.DOM.undo();
    },

    async function testAfterSequentially() {
      await setContainerQueryText({
        range: containerQueryRange,
        text: '(min-height: 20px)'
      });
      await dp.DOM.undo();
    },
  ]);
})
