(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `<html><body>contents</body></html>`, 'Tests that copying + editing <body> does not crash.');
  const {result: {root: {nodeId}}} = await dp.DOM.getDocument();
  const {result: body} = await dp.DOM.querySelector({nodeId, selector: 'body'});
  const {result: html} = await dp.DOM.querySelector({nodeId, selector: 'html'});

  const {result: {nodeId: newId}} =
      await dp.DOM.copyTo({nodeId: body.nodeId, targetNodeId: html.nodeId});
  testRunner.log(
      await dp.DOM.setOuterHTML({nodeId: newId, outerHTML: '<a>a</a>'}));
  testRunner.log(
      await dp.DOM.setOuterHTML({nodeId: newId, outerHTML: '<b>b</b>'}));
  testRunner.log('Survived');
  testRunner.completeTest();
});
