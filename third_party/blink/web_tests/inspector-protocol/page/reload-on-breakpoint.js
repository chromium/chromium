(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that reloading while paused at a breakpoint doesn\'t execute code after the breakpoint.');

  await Promise.all([
    dp.Runtime.enable(),
    dp.Debugger.enable(),
    dp.Page.enable(),
  ]);

  // Start function with debugger statement and endless loop.
  session.evaluate(`
    function hotFunction() {
      debugger;
      while(true) {};
    }
    setTimeout(hotFunction, 0);
  `);

  testRunner.log('Waiting for breakpoint...');
  await dp.Debugger.oncePaused();

  testRunner.log('Reloading page...');
  await dp.Page.reload();
  dp.Page.setLifecycleEventsEnabled({enabled: true});
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  testRunner.log('Page reloaded successfully');
  testRunner.completeTest();
})
