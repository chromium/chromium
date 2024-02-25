(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that Input.imeSetComposition works and waits for JavaScript event handlers to finish.`);
  let { init } = await testRunner.loadScript('resources/composition-test-helper.js');
  init(session, 'すしおに');

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  testRunner.log('Dispatching event');
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.imeSetComposition({
    text: 'オニ',
    selectionStart: 2,
    selectionEnd: 2,
    replacementStart: 0,
    replacementEnd: 1,
  }));
  dumpError(await dp.Input.insertText({
    text: 'オニ',
  }));


  testRunner.log(await session.evaluate('window.logs.join("\\n")'));
  testRunner.completeTest();
})
