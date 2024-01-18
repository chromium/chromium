(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that Input.insertText waits for JavaScript event handlers to finish.`);

  await session.evaluate(`
    window.input = document.createElement('input');
    document.body.appendChild(window.input);
    window.input.focus();

    window.input.addEventListener('input', pauseEvent);

    function pauseEvent(event) {
      debugger;
    }
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }


  let resolved = false;
  await dp.Debugger.enable();
  testRunner.log('Dispatching event');
  let inputEventPromise = dp.Input.insertText({
    text: 'Hello World! ❤️🚀'
  });
  inputEventPromise.then(() => resolved = true);
  await dp.Debugger.oncePaused();

  await Promise.resolve(); // just in case

  testRunner.log(resolved ? `inputEventPromise was resolved too early` : `inputEventPromise has not resolved yet`)
  testRunner.log('Paused on debugger statement');

  testRunner.log(`Input Value: ${await session.evaluate(`window.input.value`)}`);
  await dp.Debugger.resume();
  testRunner.log('Resumed');
  dumpError(await inputEventPromise);
  testRunner.log(`Recieved ack for input event`);




  testRunner.completeTest();
})
