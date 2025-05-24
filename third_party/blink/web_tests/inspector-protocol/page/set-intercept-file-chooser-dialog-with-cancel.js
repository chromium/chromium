(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Page.setInterceptFileChooserDialog can cancel dialog');

  await dp.Page.enable();
  await dp.Page.setInterceptFileChooserDialog(
      {enabled: true, cancel: true});

  const cancelEvent = await session.evaluateAsyncWithUserGesture(() => {
    return new Promise(resolve => {
      const picker = document.createElement('input');
      picker.type = 'file';
      picker.addEventListener('cancel', (event) => {
        resolve(event);
      });
      picker.click();
    });
  });

  testRunner.log(cancelEvent, "Input's DOM cancel event: ");

  testRunner.completeTest();
})
