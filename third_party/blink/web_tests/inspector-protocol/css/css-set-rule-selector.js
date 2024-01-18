(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-rule-selector.css')}'/>
`, 'Tests CSS.setRuleSelector method');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;

  var setRuleSelector = cssHelper.setRuleSelector.bind(cssHelper, styleSheetId, false);
  var verifyProtocolError = cssHelper.setRuleSelector.bind(cssHelper, styleSheetId, true);

  var firstSelectorRange = {
      startLine: 0,
      startColumn: 0,
      endLine: 0,
      endColumn: 1
  };
  var secondSelectorRange = {
      startLine: 4,
      startColumn: 0,
      endLine: 4,
      endColumn: 18
  };
  var thirdSelectorRange = {
      startLine: 12,
      startColumn: 0,
      endLine: 12,
      endColumn: 14
  };
  var forthSelectorRange = {
      startLine: 12,
      startColumn: 36,
      endLine: 13,
      endColumn: 4
  };

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  testRunner.runTestSuite([
      async function testEditSimpleSelector() {
        await setRuleSelector({
          range: firstSelectorRange,
          selector: '.EDITED-SELECTOR'
        });
        await dp.DOM.undo();
      },

      async function testEditCopmlexSelector() {
        await setRuleSelector({
          range: secondSelectorRange,
          selector: '.EDITED-SELECTOR:first-line'
        });
        await dp.DOM.undo();
      },

      async function testEditSequentially() {
        var newSelectorText = '.EDITED';
        var oldSelectorLength = thirdSelectorRange.endColumn - thirdSelectorRange.startColumn;
        var lengthDelta = newSelectorText.length - oldSelectorLength;
        await setRuleSelector({
          range: thirdSelectorRange,
          selector: newSelectorText,
        });

        var range = {
          startLine: forthSelectorRange.startLine,
          startColumn: forthSelectorRange.startColumn + lengthDelta,
          endLine: forthSelectorRange.endLine,
          endColumn: forthSelectorRange.endColumn
        };
        await setRuleSelector({
          range: range,
          selector: '.EDITED-2'
        });
        await dp.DOM.undo();
      },

      async function testInvalidSelectorText() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: '123'
        });
      },

      async function testEmptySelector() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: ''
        });
      },

      async function testSelectorTextWithUnclosedComment() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: 'body /*'
        });
      },

      async function testSelectorTextWithBrace() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: 'body {'
        });
      },

      async function testSelectorTextWithBraces() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: 'body {}'
        });
      },

      async function testSelectorTextWithExtraRule() {
        await verifyProtocolError({
          range: firstSelectorRange,
          selector: 'body {} body'
        });
      },

      async function testEditSubSelector() {
        await verifyProtocolError({
          range: { startLine: 12, startColumn: 8, endLine: 12, endColumn: 14 },
          selector: 'line'
        });
      },

      async function testInvalidParameters() {
        await verifyProtocolError({
          range: { startLine: 'three', startColumn: 0, endLine: 4, endColumn: 0 },
          selector: 'no matter what is here'
        });
      },

      async function testNegativeRangeParameters() {
        await verifyProtocolError({
          range: { startLine: -1, startColumn: -1, endLine: 1, endColumn: 0 },
          selector: 'html > body > div'
        });
      },

      async function testStartLineOutOfBounds() {
        await verifyProtocolError({
          range: { startLine: 100, startColumn: 0, endLine: 100, endColumn: 0 },
          selector: 'html > body > div'
        });
      },

      async function testEndLineOutOfBounds() {
        await verifyProtocolError({
          range: { startLine: 0, startColumn: 0, endLine: 100, endColumn: 0 },
          selector: 'html > body > div'
        });
      },

      async function testStartColumnBeyondLastLineCharacter() {
        await verifyProtocolError({
          range: { startLine: 3, startColumn: 1000, endLine: 3, endColumn: 1000 },
          selector: 'html > body > div'
        });
      },

      async function testEndColumnBeyondLastLineCharacter() {
        await verifyProtocolError({
          range: { startLine: 3, startColumn: 0, endLine: 3, endColumn: 1000 },
          selector: 'html > body > div'
        });
      },

      async function testInsertBeyondLastCharacterOfLastLine() {
        await verifyProtocolError({
          range: { startLine: 3, startColumn: 2, endLine: 3, endColumn: 2 },
          selector: 'html > body > div'
        });
      },

      async function testReversedRange() {
        await verifyProtocolError({
          range: { startLine: 2, startColumn: 0, endLine: 0, endColumn: 0 },
          selector: 'html > body > div'
        });
      },
 ]);
})
