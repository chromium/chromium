(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Runtime.queryObjects.');

  await dp.Runtime.evaluate({expression: 'document.body'});
  let {result:{result:{objectId}}} = await dp.Runtime.evaluate({
    expression: 'HTMLBodyElement.prototype'
  });
  let {result:{objects}} = await dp.Runtime.queryObjects({
    prototypeObjectId: objectId
  });
  let {result:{result}} = await dp.Runtime.callFunctionOn({
    objectId: objects.objectId,
    functionDeclaration: 'function(){ return this.map(n => n.constructor.name);}',
    returnByValue: true
  });
  testRunner.log(result.value);
  await dp.HeapProfiler.collectGarbage();
  ({result:{result}} = await dp.Runtime.callFunctionOn({
    objectId: objects.objectId,
    functionDeclaration: 'function(){ return this.map(n => n.tagName);}',
    returnByValue: true
  }));
  testRunner.log(result.value);
  testRunner.completeTest();
})
