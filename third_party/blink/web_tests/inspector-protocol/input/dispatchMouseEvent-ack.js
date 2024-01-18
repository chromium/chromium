(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that Input.dispatchMouseEvent waits for JavaScript event handlers to finish.`);

  await session.evaluate(`
    window.got = 'nope';
    window.addEventListener('mousedown', pauseEvent);
    window.addEventListener('mouseup', pauseEvent);
    window.addEventListener('mousemove', pauseEvent);

    function pauseEvent(event) {
      debugger;
      event.preventDefault();
    }
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  let buttons = 0;
  for (var event of ['mousePressed', 'mouseReleased', 'mouseMoved']) {
    testRunner.log(`-- ${event} --`);
    let resolved = false;
    await dp.Debugger.enable();
    testRunner.log('Dispatching event');
    if (event == 'mouseReleased')
      buttons = 1;
    let mouseEventPromise = dp.Input.dispatchMouseEvent({
      type: event,
      button: 'left',
      buttons: buttons,
      clickCount: 1,
      x: 100,
      y: 200
    });
    mouseEventPromise.then(() => resolved = true);
    await dp.Debugger.oncePaused();

    await Promise.resolve(); // just in case

    testRunner.log(resolved ? `mouseEventPromise for ${event} was resolved too early` : `mouseEventPromise for ${event} has not resolved yet`)
    testRunner.log('Paused on debugger statement');

    await dp.Debugger.resume();
    testRunner.log('Resumed');
    dumpError(await mouseEventPromise);
    testRunner.log(`Recieved ack for ${event}`);
  }


  testRunner.completeTest();
})
