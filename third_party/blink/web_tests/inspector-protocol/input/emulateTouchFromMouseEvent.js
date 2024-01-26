(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests Input.emulateTouchFromMouseEvent method.`);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    var expectedEventCount = -2;
    var eventCount = 0;
    var resolve;
    var gotEventsPromise = new Promise(f => resolve = f);

    function logEvent(event) {
      event.preventDefault();
      log('-----Event-----');
      log('type: ' + event.type);
      if (event.shiftKey)
        log('shiftKey');
      log('----Touches----');
      if (event.touches) {
        for (var i = 0; i < event.touches.length; i++) {
          var touch = event.touches[i];
          log('id: ' + i);
          log('pageX: ' + touch.pageX);
          log('pageY: ' + touch.pageY);
          log('radiusX: ' + touch.radiusX);
          log('radiusY: ' + touch.radiusY);
          log('rotationAngle: ' + touch.rotationAngle);
          log('force: ' + touch.force);
        }
      }
      eventCount++;
      if (eventCount === expectedEventCount)
        resolve(logs.join('\\n'));
    }

    window.addEventListener('touchstart', logEvent, {passive: false});
    window.addEventListener('touchmove', logEvent, {passive: false});
    window.addEventListener('touchend', logEvent);
    window.addEventListener('touchcancel', logEvent);
  `);

  var events = [
    {
      // Should not produce any touch events.
      'type': 'mouseMoved',
      'button': 'none',
      'modifiers': 8, // shift
      'x': 50,
      'y': 150
    },
    {
      // Should not produce any touch events.
      'type': 'mouseWheel',
      'button': 'none',
      'x': 100,
      'y': 200,
      'deltaX': 50,
      'deltaY': 70
    },
    {
      'type': 'mousePressed',
      'button': 'left',
      'clickCount': 1,
      'x': 100,
      'y': 200
    },
    {
      'type': 'mouseMoved',
      'button': 'left',
      'clickCount': 1,
      'x': 100,
      'y': 250
    },
    {
      'type': 'mouseReleased',
      'button': 'left',
      'clickCount': 1,
      'x': 100,
      'y': 250
    },
    {
      'type': 'mousePressed',
      'button': 'left',
      'clickCount': 1,
      'x': 100,
      'y': 200
    },
    {
      'type': 'mousePressed',
      'button': 'forward',
      'clickCount': 1,
      'x': 100,
      'y': 200
    },
    {
      'type': 'mousePressed',
      'button': 'back',
      'clickCount': 1,
      'x': 100,
      'y': 200
    }
  ];

  await dp.Emulation.setTouchEmulationEnabled({enabled: true});
  await dp.Emulation.setEmitTouchEventsForMouse({enabled: true});

  // Moving mouse while not pressed and press with button with forward/back
  // button does not generate touch events.
  await session.evaluate(`expectedEventCount = ${events.length - 4}`);

  var time = Number(new Date()) / 1000;
  for (var index = 0; index < events.length; index++) {
    var event = events[index];
    event.timestamp = time + index * 100;
    var msg = await dp.Input.emulateTouchFromMouseEvent(event);
    if (msg.error)
      testRunner.log('Error: ' + msg.error.message);
  }

  testRunner.log(await session.evaluateAsync('gotEventsPromise'));
  testRunner.completeTest();
})
