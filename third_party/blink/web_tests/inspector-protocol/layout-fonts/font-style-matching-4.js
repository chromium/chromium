(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var test = await testRunner.loadScript('resources/style-matching-test.js');
  test(testRunner);
})
