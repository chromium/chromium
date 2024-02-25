(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that passing a string intestead of a protocol object does not crash crash backend (crbug.com/1154370)`);

  const response = await dp.Network.setExtraHTTPHeaders({headers: 'a string'});
  if (response.error && response.error.data)
    response.error.data = _trimErrorMessage(response.error.data);
  testRunner.log(response, `Error: `);
  testRunner.completeTest();

  function _trimErrorMessage(error) {
    return error.replace(/at position \d+/, "<somewhere>");
  }
})
