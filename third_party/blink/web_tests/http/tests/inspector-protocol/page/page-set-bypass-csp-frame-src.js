(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests that Page.setBypassCSP works for main frame.`);

  await dp.Log.enable();
  await dp.Page.enable();
  await dp.Runtime.enable();
  dp.Log.onEntryAdded(result => testRunner.log(result.params.entry.text));

  testRunner.log('Verify frame-src without bypass');
  await page.navigate('./resources/csp-frame-src.php');

  await dp.Page.setBypassCSP({ enabled: true });

  testRunner.log('Verify frame-src with bypass');
  await page.navigate('./resources/csp-frame-src.php');

  testRunner.completeTest();
})
