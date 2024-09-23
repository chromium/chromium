(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <style>
  #no-properties {
    @supports (display: flex) {
      & {
        color: red;
      }
    }
  }

  #with-properties {
    @supports (display: flex) {
      color: blue;
      & {
        color: red;
      }
    }
  }

  #commented-out-property {
    @supports (display: flex) {
      /* color: red; */
    }
  }
  </style>
  <div id='no-properties'></div>
  <div id='with-properties'></div>
  <div id='commented-out-property'></div>`,
'The test verifies functionality of protocol method CSS.getMatchedStylesForNode for nested groups.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  // Test that when there are no properties inside a nested rule, it won't have an
  // empty CSS rule.
  testRunner.log("There shouldn't be an empty rule for #no-properties");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#no-properties');

  testRunner.log("\nThere should be a rule for implicit nested group.");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#with-properties');

  testRunner.log("\nThere should not be a rule for #commented-out-property.");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#commented-out-property');

  testRunner.completeTest();
});
