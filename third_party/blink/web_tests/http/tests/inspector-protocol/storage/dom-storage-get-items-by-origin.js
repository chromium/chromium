(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that getting DOMStorage items by origin doesn't return an error response\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  const securityOrigin = (await dp.Page.getResourceTree()).result.frameTree.frame.securityOrigin;
  const storageId = {securityOrigin, isLocalStorage: true};
  // clear storage to avoid leakage from other tests
  await dp.DOMStorage.clear({storageId});

  const items = (await dp.DOMStorage.getDOMStorageItems({storageId})).result;

  testRunner.log("Get DOM storage items");
  testRunner.log(items);

  testRunner.completeTest();
})
