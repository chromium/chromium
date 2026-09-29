(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that editing a detached DOM element does not crash.');
  await dp.DOM.getDocument();
  const {result: {result: {objectId}}} = await dp.Runtime.evaluate({
    expression: 'document.createElement("div")',
  });
  const {result: {nodeId}} = await dp.DOM.requestNode({objectId});
  const setNodeNameResult =
      await dp.DOM.setNodeName({nodeId, name: 'whatever'});
  testRunner.log(setNodeNameResult);
  testRunner.log(await dp.DOM.setOuterHTML({
    nodeId: setNodeNameResult.result.nodeId,
    outerHTML: '<span></span>',
  }));
  testRunner.log('Survived');
  testRunner.completeTest();
});
