(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that getting storageKey for frame with opaque origin throws an error\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  page.loadHTML("<iframe src='about:blank' sandbox></iframe>");
  const attachmentPromise = dp.Page.onceFrameAttached();
  const id = (await attachmentPromise).params.frameId;

  const response = await dp.Storage.getStorageKeyForFrame({frameId: id});
  if (response.error) {
   testRunner.log('Throws an expected error: ' + response.error.message);
  }
  testRunner.completeTest();
})
