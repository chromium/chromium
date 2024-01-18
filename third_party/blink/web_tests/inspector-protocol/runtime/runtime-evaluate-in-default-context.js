(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that default execution context is selected correctly.`);

  await session.evaluateAsync(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/runtime-events-iframe.html')}';
    document.body.appendChild(frame);
    new Promise(f => frame.onload = f)
  `);

  var result = await dp.Runtime.evaluate({expression: 'window.a' });
  testRunner.log(result);
  testRunner.completeTest();
})
