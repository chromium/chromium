(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<style>#test {
    box-sizing: border-box;
}

#test {
    /* resetting some properties */
    line-height: 1;
    font-family: "Arial";
    color: blue;
    display: flex; /* flex FTW! */
}

@media (min-width: 1px) {
    #test {
        font-size: 200%;
    }

    #test {
    }
}

</style>
<article id='test'></article>`, 'The test verifies functionality of protocol method CSS.addRule.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  dp.CSS.enable();
  var result = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = result.params.header.styleSheetId;

  async function addRuleAndUndo(options) {
    await cssHelper.addRule(styleSheetId, false, options);
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test')
    await dp.DOM.undo();
  }

  var verifyProtocolError = cssHelper.addRule.bind(cssHelper, styleSheetId, true);

  var response = await dp.CSS.getStyleSheetText({styleSheetId: styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  testRunner.runTestSuite([
    /* Tests that add rule into style sheet. */
    async function testAddRuleToStyleSheetBeginning() {
      await addRuleAndUndo({
        location: {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddRuleToStyleSheetEnding() {
      await addRuleAndUndo({
        location: {startLine: 20, startColumn: 0, endLine: 20, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`});
    },

    async function testAddRuleToStyleSheetCenter() {
      await addRuleAndUndo({
        location: {startLine: 11, startColumn: 0, endLine: 11, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`});
    },

    async function testAddRuleToRuleEnding() {
      await addRuleAndUndo({
        location: {startLine: 2, startColumn: 1, endLine: 2, endColumn: 1},
        ruleText: `#test{\n    content: 'EDITED';\n}`});
    },

    async function testAddRuleWithFormatting() {
      await addRuleAndUndo({
        location: {startLine: 2, startColumn: 1, endLine: 2, endColumn: 1},
        ruleText: `\n\n#test{\n    content: 'EDITED';\n}`});
    },

    /* Tests that add rule into MediaRule. */

    async function testAddRuleToMediaRuleBeginning() {
      await addRuleAndUndo({
        location: {startLine: 12, startColumn: 25, endLine: 12, endColumn: 25},
        ruleText: `#test { content: 'EDITED'; }`});
    },

    async function testAddRuleToMediaRuleCenter() {
      await addRuleAndUndo({
        location: {startLine: 16, startColumn: 0, endLine: 16, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`});
    },

    async function testAddRuleToMediaRuleEnd() {
      await addRuleAndUndo({
        location: {startLine: 19, startColumn: 0, endLine: 19, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`});
    },

    /* Tests that verify error reporting. */

    async function testInvalidRule() {
      await verifyProtocolError({
        location: {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0},
        ruleText: `#test { content: 'EDITED';`,
      });
    },

    async function testInvalidRule2() {
      await verifyProtocolError({
        location: {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0},
        ruleText: '@media all { ',
      });
    },

    async function testInvalidRule3() {
      await verifyProtocolError({
        location: {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0},
        ruleText: '#test {} #test {',
      });
    },

    async function testAddingRuleInsideSelector() {
      await verifyProtocolError({
        location: {startLine: 0, startColumn: 2, endLine: 0, endColumn: 2},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddingRuleBeforeRuleBody() {
      await verifyProtocolError({
        location: {startLine: 4, startColumn: 6, endLine: 4, endColumn: 6},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddingRuleInsideMedia1() {
      await verifyProtocolError({
        location: {startLine: 12, startColumn: 9, endLine: 12, endColumn: 9},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddingRuleInsideMedia2() {
      await verifyProtocolError({
        location: {startLine: 12, startColumn: 15, endLine: 12, endColumn: 15},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddingRuleBeforeMediaBody() {
      await verifyProtocolError({
        location: {startLine: 12, startColumn: 24, endLine: 12, endColumn: 24},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },

    async function testAddingRuleInsideStyleRule() {
      await verifyProtocolError({
        location: {startLine: 18, startColumn: 0, endLine: 18, endColumn: 0},
        ruleText: `#test { content: 'EDITED'; }`,
      });
    },
  ]);
})
