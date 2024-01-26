(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(`Tests Input.dispatchKeyEvent commands option.`);

  await session.evaluate(`
    const textarea = document.createElement('textarea');
    document.body.appendChild(textarea);
    textarea.value = 'hello world';
    textarea.focus();

    function selectedText() {
      const textarea = document.querySelector('textarea');
      return textarea.value + '\\n' + ' '.repeat(textarea.selectionStart) + '~'.repeat(textarea.selectionEnd-textarea.selectionStart) + '^'
    }
  `);

  async function dumpErrorAndLogs(options) {
    testRunner.log('');
    testRunner.log(`Sending "${options.key}"`);
    if (options.commands && options.commands.length)
      testRunner.log(`with commands ${JSON.stringify(options.commands)}`)
    const message = await dp.Input.dispatchKeyEvent(options);
    if (message.error)
      testRunner.log('Error: ' + message.error.message);

    testRunner.log(await session.evaluate(`selectedText()`));
  }

  await dumpErrorAndLogs({
    type: 'keyDown',
    key: 'b',
    modifiers: 2,
    commands: ['selectAll'],
    windowsVirtualKeyCode: 66,
    code: 'KeyB'
  });
  await dumpErrorAndLogs({
    type: 'keyDown',
    windowsVirtualKeyCode: 8,
    key: 'Backspace',
    code: 'Backspace'
  });

  await dumpErrorAndLogs({
    type: 'keyDown',
    key: 'c',
    text: 'c',
    unmodifiedText: 'c',
    commands: [],
    windowsVirtualKeyCode: 67,
    code: 'KeyC'
  });

  testRunner.log('');
  testRunner.log('Canceling the next keydown');

  await session.evaluate(`
    window.addEventListener('keydown', event => event.preventDefault(), {once: true});
  `);
  await dumpErrorAndLogs({
    type: 'keyDown',
    key: 'd',
    modifiers: 2,
    commands: ['selectAll'],
    windowsVirtualKeyCode: 67,
    code: 'KeyC'
  });
  await dumpErrorAndLogs({
    type: 'keyDown',
    key: 'e',
    text: 'e',
    unmodifiedText: 'e',
    commands: [],
    windowsVirtualKeyCode: 68,
    code: 'KeyE'
  });

  testRunner.completeTest();
});
