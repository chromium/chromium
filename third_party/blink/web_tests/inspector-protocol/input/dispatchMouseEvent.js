(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests Input.dispatchMouseEvent method.`);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      log('button: ' + event.button);
      log('buttons: ' + event.buttons);
      if (event.shiftKey)
        log('shiftKey');
      log('x: ' + event.x);
      log('y: ' + event.y);
      if (event.type === 'mousewheel') {
        log('deltaX: ' + event.deltaX);
        log('deltaY: ' + event.deltaY);
      }
      event.preventDefault();
    }

    window.addEventListener('mousedown', logEvent);
    window.addEventListener('mouseup', logEvent);
    window.addEventListener('mousemove', logEvent);
    window.addEventListener('contextmenu', logEvent);
    window.addEventListener('mousewheel', logEvent);
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 0,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'middle',
    buttons: 1,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'middle',
    buttons: 5,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseMoved',
    modifiers: 8, // shift
    buttons: 0,
    x: 50,
    y: 150
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'right',
    buttons: 0,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'right',
    buttons: 2,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'back',
    buttons: 0,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'forward',
    buttons: 8,
    clickCount: 2,
    x: 100,
    y: 200
  }));
  dumpError(await dp.Input.dispatchMouseEvent({
    type: 'mouseWheel',
    x: 100,
    y: 200,
    deltaX: 50,
    deltaY: 70
  }));

  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  testRunner.completeTest();
})
