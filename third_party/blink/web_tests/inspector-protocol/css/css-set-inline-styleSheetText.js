(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<div id='inliner' style='color: red;'></div>`, 'Verify that CSS.setStyleSheetText works for inline styles.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#inliner');

  var response = await dp.CSS.getInlineStylesForNode({nodeId});
  var styleSheetId = response.result.inlineStyle.styleSheetId;

  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log(response.result.text);

  await dp.CSS.setStyleSheetText({styleSheetId, text: 'border: 1px solid black;'});
  var response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log(response.result.text);
  testRunner.completeTest();
})
