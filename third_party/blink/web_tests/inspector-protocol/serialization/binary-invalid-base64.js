(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that passing an invalid base64 string as binary casues an error to be reported`);

  function trimErrorMessage(message) {
    return message.replace(/at position \d+/, "<somewhere>");
  }
  await dp.Page.enable();
  const {error} = await dp.Page.addCompilationCache({ url: 'http://example.com/hello.js', data: '$#@%&'});
  testRunner.log(trimErrorMessage(error.data));
  testRunner.completeTest();
})
