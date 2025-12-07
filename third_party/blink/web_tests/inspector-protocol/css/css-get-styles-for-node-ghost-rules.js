(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <style>
  #comments {
    top: 10px;
    .other1 {}
    /* margin-left: 10px; */
    .other2 {}
    /* margin-right: 10px; */
  }
  #invalid {
    top: 10px;
    .other1 {}
    -webkit-unsupported-left: 10px;
    .other2 {}
    -webkit-unsupported-right: 10px;
  }
  #group-rules {
    top: 10px;
    @media (width > 100px) {
      /* padding-left: 20px; */
    }
    /* margin-left: 10px; */
    @media (width > 100px) {
      -webkit-unsupported-left: 20px;
    }
    /* margin-right: 10px; */
  }
  </style>
  <div id='comments'></div>
  <div id='invalid'></div>
  <div id='group-rules'></div>
`,
'The test verifies functionality of protocol method CSS.getMatchedStylesForNode for nested groups.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log("Comments with declarations should cause ghost rules (#comments)");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#comments');

  testRunner.log("Invalid declarations should cause ghost rules (#invalid)");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#invalid');

  testRunner.log("Ghost rules can appear in nested group rules (#group-rules)");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#group-rules');

  testRunner.completeTest();
});
