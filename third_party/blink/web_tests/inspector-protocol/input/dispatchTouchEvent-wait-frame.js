(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {session, dp} = await testRunner.startBlank(`Tests that Input.dispatchTouchEvent waits for a frame before comitting.`);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  let resolved = false;
  await dp.Debugger.enable();
  testRunner.log('Dispatching event');
  await session.evaluate(`
    window.logs = [];
    requestAnimationFrame(() => {
      logs.push('requestAnimationFrame');
      setTimeout(() => logs.push('setTimeout'), 0);
      debugger;
    });
    window.addEventListener('touchstart', () => logs.push('touchstart'));
  `);
  let touchEventPromise = dp.Input.dispatchTouchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  touchEventPromise.then(() => resolved = true);
  await dp.Debugger.oncePaused();
  await new Promise(x => setTimeout(x, 100)); // wait a bit to see if the touchEventPromise will resolve early
  testRunner.log(resolved ? `touchEventPromise was resolved too early` : `touchEventPromise for has not resolved yet`)
  testRunner.log('Paused on debugger statement');

  await dp.Debugger.resume();
  testRunner.log('Resumed');
  dumpError(await touchEventPromise);

  testRunner.log(`Recieved ack`);

  testRunner.log(`Order of events:`);
  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));

  testRunner.completeTest();
})
