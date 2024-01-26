(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
      <link rel='stylesheet' href='${testRunner.url('resources/visited-link.css')}'/>
      <a id='inspected' href=''></a>`,
      'The test verifies functionality of protocol method CSS.getMatchedStylesForNode for visited links');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  await cssHelper.requestNodeId(documentNodeId, '#inspected');

  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#inspected');
  testRunner.completeTest();
});

