(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
      <link rel='stylesheet' href='${testRunner.url('resources/set-active-property-value.css')}'/>
      <div id='parent-div' style='padding-top: 20px;'>
          <div id='inspected' style='padding-top: 55px; margin-top: 33px !important;'></div>
          <div id='child-div'></div>
      </div>`,
      'The test verifies functionality of protocol method CSS.getMatchedStylesForNode and CSS.getInlineStylesForNode.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  await cssHelper.requestNodeId(documentNodeId, '#inspected');

  // Test on non Element node
  let result = await dp.CSS.getInlineStylesForNode({'nodeId': documentNodeId});
  testRunner.log(result);
  result = await dp.CSS.getMatchedStylesForNode({'nodeId': documentNodeId});
  testRunner.log(result);

  // Test on Element node
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#inspected');
  testRunner.completeTest();
});

