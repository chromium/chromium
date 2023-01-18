(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that getting DOMStorage items by origin doesn't return an error response\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  const securityOrigin = (await dp.Page.getResourceTree()).result.frameTree.frame.securityOrigin;
  const storageId = {securityOrigin, isLocalStorage: true};

  const items = (await dp.DOMStorage.getDOMStorageItems({storageId})).result;

  testRunner.log("Get DOM storage items");
  testRunner.log(items);

  testRunner.completeTest();
})
