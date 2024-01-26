(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests Input.dispatchKeyEvent method.`);

  await session.evaluate(`
    window.addEventListener('keydown', logEvent);
    window.addEventListener('keypress', logEvent);
    window.addEventListener('keyup', logEvent);

    window.logs = [];
    function log(text) {
      logs.push(text);
    }

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      if (event.altKey)
        log('altKey');
      if (event.ctrlKey)
        log('ctrlKey');
      if (event.metaKey)
        log('metaKey');
      if (event.shiftKey)
        log('shiftKey');
      if (event.keyCode)
        log('keyCode: ' + event.keyCode);
      if (event.key)
        log('key: ' + event.key);
      if (event.charCode)
        log('charCode: ' + event.charCode);
      if (event.text)
        log('text: ' + event.text);
      if (event.location)
        log('location: ' + event.location);
      if (event.code)
        log('code: ' + event.code);
      log('');
    }
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    windowsVirtualKeyCode: 65, // VK_A
    key: 'A'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'char',
    modifiers: 8, // shift
    text: 'A',
    unmodifiedText: 'a'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    windowsVirtualKeyCode: 65,
    key: 'A'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'char',
    text: '\u05E9',  // Hebrew Shin (sh)
    unmodifiedText: '\u05E9'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyDown',
    modifiers: 8, // shift
    windowsVirtualKeyCode: 16,
    location: 1,
    code: 'ShiftLeft'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyDown',
    modifiers: 8, // shift
    windowsVirtualKeyCode: 16,
    location: 2,
    code: 'ShiftRight'
  }));

  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));

  testRunner.log('Expect error for invalid text or unmodifiedText:')
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'char',
    text: 'oops',
    unmodifiedText: 'SNAP'
  }));

  testRunner.completeTest();
})
