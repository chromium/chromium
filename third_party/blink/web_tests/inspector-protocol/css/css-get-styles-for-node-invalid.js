(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
      <link rel='stylesheet' href='${testRunner.url('resources/set-active-property-value-invalid.css')}'/>
      <div id='parent-div'>
          <div id='inspected'></div>
      </div>`,
      'The test verifies functionality of protocol method CSS.getMatchedStylesForNode when the CSS contains invalid @ rules');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  await cssHelper.requestNodeId(documentNodeId, '#inspected');

  // Test on Element node
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');
  testRunner.completeTest();
});
