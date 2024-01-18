(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests timestamps in multiple input domain methods.`);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    var expectedOffsets = [];
    var receivedTimestamps = [];
    var resolve;
    var verifyTimestampsPromise = new Promise(f => resolve = f);

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      receivedTimestamps.push(event.timeStamp);
      if (receivedTimestamps.length === expectedOffsets.length)
        verifyTimestamps();
    }

    function verifyTimestamps() {
      log('-----Verify-----');
      log('Received ' + receivedTimestamps.length + ' timestamps');

      // Event.timeStamp values are in milliseconds
      var receivedOffsets = receivedTimestamps.map(t => t - receivedTimestamps[0]);
      for (var i = 0; i < receivedOffsets.length; ++i) {
        if (isNear(receivedOffsets[i], expectedOffsets[i]))
          log('timeStamps offsets is as expected.');
        else
          log('timeStamp offset is expected ' + expectedOffsets[i] + ' but it is:' + receivedOffsets[i]);
      }

      function isNear(a, b) {
        var epsilon = 5;
        return Math.abs(b - a) < epsilon;
      }

      resolve(logs.join('\\n'));
    }

    window.addEventListener('keydown', logEvent);
    window.addEventListener('mousedown', logEvent);
    window.addEventListener('touchstart', logEvent);
  `);

  // We send epoch timestamp but expect to receive high-res timestamps
  var baseEpochTimestamp = Date.now() / 1000; // in seconds
  var offsets = [0, 5, 10, 15];
  var sentTimestamps = offsets.map(offset => baseEpochTimestamp + offset);

  var offsetsMs = offsets.map(offset => 1000 * offset);
  await session.evaluate(`
    expectedOffsets = [${offsetsMs.join(', ')}];
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.fail(message.error.message);
  }

  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    timestamp: sentTimestamps[0]
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    timestamp: sentTimestamps[1]
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    timestamp: sentTimestamps[2],
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    timestamp: sentTimestamps[3],
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 200
  }));
  testRunner.log(await session.evaluateAsync('verifyTimestampsPromise'));
  testRunner.completeTest();
})
