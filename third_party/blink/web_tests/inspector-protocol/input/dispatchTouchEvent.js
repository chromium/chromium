(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests Input.dispatchTouchEvent method.`);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    function takeLogs() {
      var result = logs.join('\\n');
      logs = [];
      return result;
    }

    function logEvent(event) {
      event.preventDefault();
      log('-----Event-----');
      log('type: ' + event.type);
      if (event.shiftKey)
        log('shiftKey');
      log('----Touches----');
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

    window.addEventListener('touchstart', logEvent, {passive: false});
    window.addEventListener('touchend', logEvent, {passive: false});
    window.addEventListener('touchmove', logEvent, {passive: false});
    window.addEventListener('touchcancel', logEvent, {passive: false});
  `);

  async function dispatchEvent(params) {
    testRunner.log('\nDispatching event:');
    testRunner.log(params);
    var response = await dp.Input.dispatchTouchEvent(params);
    if (response.error)
      testRunner.log(response);
    testRunner.log(await session.evaluate(`takeLogs()`));
  }

  testRunner.log('\n\n------- Sequence ------');
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

  testRunner.log('\n------- Sequence ------');
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
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  testRunner.log('\n------- Sequence ------');
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

  testRunner.log('\n------- Sequence ------');
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  testRunner.log('\n------- Sequence ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: []
  });

  testRunner.log('\n------- Sequence ------');
  await dispatchEvent({
    type: 'touchStart',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
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

  testRunner.log('\n------- Sequence ------');
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
  });
  await dispatchEvent({
    type: 'touchMove',
    touchPoints: [{
      x: 100,
      y: 100
    }]
  });
  await dispatchEvent({
    type: 'touchEnd',
    touchPoints: []
  });

  testRunner.log('\n------- Sequence ------');
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

  testRunner.completeTest();
})
