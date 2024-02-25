(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests default timestamp in Input.emulateTouchFromMouseEvent uses monotonic clock.`);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    var resolve;
    var verifyTimestampsPromise = new Promise(f => resolve = f);
    var initalTimestamp = performance.now();

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);  // Should be mousedown
      verifyEventTimestamp(event.timeStamp);
    }

    function verifyEventTimestamp(eventTimestamp) {
      log('-----Verify-----');
      if (eventTimestamp > initalTimestamp) {
        log('Event timestamp is larger than initial timestamp.');
      } else {
        log('Event timestamp is not larger than initial timestamp.');
      }
      resolve(logs.join('\\n'));
    }

    window.addEventListener('touchstart', logEvent, {passive: false});
  `);
  function dumpError(message) {
    if (message.error)
      testRunner.fail(message.error.message);
  }

  await dp.Emulation.setTouchEmulationEnabled({enabled: true});
  await dp.Emulation.setEmitTouchEventsForMouse({enabled: true});

  // We leave out timestamp as a param so default timestamp would be used.
  dumpError(await dp.Input.emulateTouchFromMouseEvent({
    type: 'mousePressed',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 200
  }));

  testRunner.log(await session.evaluateAsync('verifyTimestampsPromise'));
  testRunner.completeTest();
})
