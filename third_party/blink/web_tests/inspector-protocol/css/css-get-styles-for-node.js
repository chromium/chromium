(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
      <link rel='stylesheet' href='${testRunner.url('resources/set-active-property-value.css')}'/>
      <div id='parent-div' style='padding-top: 20px;'>
          <div id='inspected' style='padding-top: 55px; margin-top: 33px !important;'></div>
          <div id='child-div'></div>
      </div>
      <div id='shorthand-div' style='margin: 0; margin-top: 5px; padding: var(--x); border: 1px solid black; flex: 1 !important;'></div>`,
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

  const shorthandNodeId = await cssHelper.requestNodeId(documentNodeId, '#shorthand-div');
  const shorthandResult = await dp.CSS.getInlineStylesForNode({'nodeId': shorthandNodeId});
  testRunner.log('checking parsed longhand components from shorthand properties');
  for (const property of shorthandResult.result.inlineStyle.cssProperties) {
    if (property.longhandProperties) {
      testRunner.log(property.longhandProperties);
    }
  }
  testRunner.completeTest();
});

