(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Runtime.getProperties doesnt crash on document.all in iframe');
  await dp.Runtime.enable();
  await dp.Debugger.enable();
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  let {params:{context:{id}}} = await dp.Runtime.onceExecutionContextCreated();
  let {result:{result:{objectId}}} = await dp.Runtime.evaluate({
    contextId: id,
    expression: 'document.all',
  });
  await dp.Runtime.getProperties({objectId});
  testRunner.completeTest();
})
