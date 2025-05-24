(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
   <div id='inspected1'
     style='--\\\\{\\\\&\\\\:a\\\\}b: red; color: var(--\\\\{\\\\&\\\\:a\\\\}b);'>
     inline style
   </div>
   <div id='inspected2'
     style='--\\\\{\\\\\\\\\\\\:a\\\\}b\\\\\\\\: red; color: var(--\\\\{\\\\\\\\\\\\:a\\\\}b\\\\\\\\);'>
     inline style
   </div>`,
      'The test verifies functionality of protocol method CSS.getMatchedStylesForNode and CSS.getInlineStylesForNode when parsing property names with escaped characters');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  let node = await cssHelper.requestNodeId(documentNodeId, '#inspected1');
  let result = await dp.CSS.getInlineStylesForNode({'nodeId': node});
  testRunner.log(result);

  node = await cssHelper.requestNodeId(documentNodeId, '#inspected2');
  result = await dp.CSS.getInlineStylesForNode({'nodeId': node});
  testRunner.log(result);

  testRunner.completeTest();
});

