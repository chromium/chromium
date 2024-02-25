(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { session, dp } = await testRunner.startBlank(`Tests Input.dispatchTouchEvent method.`);
  await dp.Runtime.addBinding({ name: 'logEvent' });
  await dp.Runtime.addBinding({ name: 'logString' });
  dp.Runtime.onBindingCalled(data => {
    if (data.params.name === 'logString') {
      testRunner.log(data.params.payload);
      if (data.params.payload === '\n------- Done ------') {
        testRunner.completeTest();
      }
    } else {
      const event = JSON.parse(data.params.payload);
      testRunner.log('-----Event-----');
      testRunner.log('type: ' + event.type);
      if (event.shiftKey)
        testRunner.log('shiftKey');
      testRunner.log('----Touches----');
      for (var i = 0; i < event.touches.length; i++) {
        var touch = event.touches[i];
        testRunner.log('id: ' + i);
        testRunner.log('pageX: ' + touch.pageX);
        testRunner.log('pageY: ' + touch.pageY);
        testRunner.log('radiusX: ' + touch.radiusX);
        testRunner.log('radiusY: ' + touch.radiusY);
        testRunner.log('rotationAngle: ' + touch.rotationAngle);
        testRunner.log('force: ' + touch.force);
      }
    }
  });
  await session.evaluate(`
    function logTouchEvent(event) {
      event.preventDefault();
      logEvent(JSON.stringify({
        type: event.type,
        shiftKey: event.shiftKey,
        touches: Array.from(event.touches, (touch) => ({
          pageX: touch.pageX,
          pageY: touch.pageY,
          radiusX: touch.radiusX,
          radiusY: touch.radiusY,
          rotationAngle: touch.rotationAngle,
          force: touch.force,
        })),
      }));
    }

    window.addEventListener('touchstart', logTouchEvent, {passive: false});
    window.addEventListener('touchend', logTouchEvent, {passive: false});
    window.addEventListener('touchmove', logTouchEvent, {passive: false});
    window.addEventListener('touchcancel', logTouchEvent, {passive: false});
  `);

  async function log(message) {
    // Pipe messages through the session to make sure they arrive in order.
    await session.evaluate(`logString(${JSON.stringify(message)})`);
  }

  async function dispatchEvent(params, shouldFail = false, numberOfExpectedEvents = 1) {
    await log('\nDispatching event:');
    await log(`${params.type} with ${params.touchPoints.length} touch points`);
    const response = await dp.Input.dispatchTouchEvent(params);
    if (response.error) {
      await log(response);
      await log('');
    }
  }

  await log('\n\n------- Sequence 1 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 200
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 200,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 2 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 20,
      y: 30,
      id: 0
    }, {
      x: 100,
      y: 200,
      radiusX: 5,
      radiusY: 6,
      rotationAngle: 1.0,
      force: 0.0,
      id: 1
    }],
    modifiers: 8 // shift
  }, false, 2);
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 3 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 20,
      y: 30,
      id: 0
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 20,
      y: 30,
      id: 0
    }, {
      x: 100,
      y: 200,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 25,
      y: 36,
      id: 0
    }, {
      x: 101,
      y: 202,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 103,
      y: 203,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 4 ------');
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 5 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: []
  });

  await log('\n------- Sequence 6 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchCancel',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchCancel',
    touchPoints: []
  });

  await log('\n------- Sequence 7 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  }, true);
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  }, true);
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 8 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchCancel',
    touchPoints: []
  });

  await log('\n------- Sequence 9 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 1
    }, {
      x: 150,
      y: 100,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 150,
      id: 1
    }, {
      x: 150,
      y: 150,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Sequence 10 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 1
    }, {
      x: 150,
      y: 100,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchCancel',
    touchPoints: []
  });

  await log('\n------- Sequence 11 ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 150,
      y: 100,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 150,
      id: 1
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: [{
      x: 150,
      y: 100,
      id: 2
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  await log('\n------- Done ------');
});
