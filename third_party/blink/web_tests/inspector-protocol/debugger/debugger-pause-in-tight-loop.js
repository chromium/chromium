(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that JS can be interrupted and paused.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  // Start tight loop in page.
  session.evaluate(`
    var terminated = false;
    function hotFunction() {
      console.log('hi');
      var message_id = 1;
      var ts = Date.now();
      while (!terminated) {
        if (Date.now() - ts > 1000) {
          ts = Date.now();
          console.error('Message #' + message_id++);
        }
      }
    }
    setTimeout(hotFunction, 0);
  `);
  testRunner.log('didEval');

  await dp.Runtime.onceConsoleAPICalled();
  testRunner.log('didFireTimer');

  dp.Debugger.pause();
  await dp.Debugger.oncePaused();
  testRunner.log('SUCCESS: Paused');

  dp.Runtime.evaluate({expression: 'terminated = true;' });
  dp.Debugger.resume();
  await dp.Debugger.onceResumed();
  testRunner.log('SUCCESS: Resumed');
  testRunner.completeTest();
})
