(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(``, 'Test that uncaught exception in MediaQueryListListener will be reported to the console. On success you should see two exceptions in the listener logged to the console (first time when the media type is overridden and second - when they are restored). https://bugs.webkit.org/show_bug.cgi?id=105162');

  await session.evaluate(() => {
    var theMediaQueryList = window.matchMedia('print');
    theMediaQueryList.addListener(() => objectThatDoesNotExist.produceError());
  });

  await dp.Runtime.enable();

  dp.Emulation.setEmulatedMedia({media: 'print'});
  var event = await dp.Runtime.onceExceptionThrown();
  testRunner.log(event.params.exceptionDetails.exception.description);

  dp.Emulation.setEmulatedMedia({ media: '' });
  var event = await dp.Runtime.onceExceptionThrown();
  testRunner.log(event.params.exceptionDetails.exception.description);

  testRunner.completeTest();
})
