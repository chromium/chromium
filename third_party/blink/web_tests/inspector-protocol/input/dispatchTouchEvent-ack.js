(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that Input.dispatchTouchEvent waits for JavaScript event handlers to finish.`);

  await session.evaluate(`
    window.got = 'nope';
    window.addEventListener('click', pauseEvent);

    function pauseEvent(event) {
      debugger;
      event.preventDefault();
    }
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  let resolved = false;
  await dp.Debugger.enable();
  await dp.Emulation.setTouchEmulationEnabled({enabled: true});
  await dp.Emulation.setEmitTouchEventsForMouse({enabled: true});
  testRunner.log('Dispatching event');
  await dp.Input.dispatchTouchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  let touchEventPromise = dp.Input.dispatchTouchEvent({
    type: 'touchEnd',
    touchPoints: []
  });
  touchEventPromise.then(() => resolved = true);
  await dp.Debugger.oncePaused();

  await Promise.resolve(); // just in case

  testRunner.log(resolved ? `touchEventPromise was resolved too early` : `touchEventPromise for has not resolved yet`)
  testRunner.log('Paused on debugger statement');

  await dp.Debugger.resume();
  testRunner.log('Resumed');
  dumpError(await touchEventPromise);
  testRunner.log(`Recieved ack`);

  testRunner.completeTest();
})
