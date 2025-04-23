(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // crbug.com/408401203
  var {page, session, dp} = await testRunner.startHTML(`
      <style>
        @layer base, utilities;
        @layer base {
          :where(#inspected) {
            @media (width > 0px) {
              color: green;
            }
          }
        }
      </style>
      <div id='inspected'></div>`,
      `Tests CSS.getMatchedStylesForNode() under @layer rules.`);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  const documentNodeId = await cssHelper.requestDocumentNodeId();

  // Don't crash:
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#inspected');
  testRunner.completeTest();
});
