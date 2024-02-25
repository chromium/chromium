(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that mouse manipulation through input domain works concurrently from multiple sessions.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();

  await session2.evaluate(`
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
      event.preventDefault();
    }

    window.addEventListener('mousedown', logEvent);
    window.addEventListener('mouseup', logEvent);
    window.addEventListener('mousemove', logEvent);
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  testRunner.log('Mouse down in session1');
  dumpError(await session1.protocol.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'middle',
    buttons: 0,
    clickCount: 1,
    x: 100,
    y: 200
  }));
  testRunner.log('Mouse move in session2');
  dumpError(await session2.protocol.Input.dispatchMouseEvent({
    type: 'mouseMoved',
    button: 'middle',
    buttons: 4,
    x: 50,
    y: 150
  }));
  testRunner.log('Mouse up in session2');
  dumpError(await session2.protocol.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'middle',
    buttons: 4,
    clickCount: 1,
    x: 50,
    y: 150
  }));
  testRunner.log('Mouse move in session1');
  dumpError(await session1.protocol.Input.dispatchMouseEvent({
    type: 'mouseMoved',
    x: 150,
    y: 50
  }));

  testRunner.log(await session1.evaluate(`window.logs.join('\\n')`));
  testRunner.completeTest();
})
