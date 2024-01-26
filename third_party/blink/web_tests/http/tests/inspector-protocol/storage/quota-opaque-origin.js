(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that reading quota an opaque origin throws an error\n`);
  await page.loadHTML("<iframe src='about:blank' sandbox></iframe>");
  const response = await dp.Storage.getUsageAndQuota({ origin: 'about:blank' });
  testRunner.log('Throws an expected error: ' + response.error.message);
  testRunner.completeTest();
})
