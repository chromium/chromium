(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests DOMStorage functionality with storageKey\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  const addedPromise = dp.DOMStorage.onceDomStorageItemAdded();
  session.evaluate('window.localStorage.setItem("testKey", "testItem")');
  const event = await addedPromise;
  const storageId = event.params.storageId;

  testRunner.log("Set item")
  testRunner.log(`storageId.storageKey: ${typeof storageId.storageKey}`)
  testRunner.log(storageId.storageKey ? "not empty\n" : "empty\n");

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const key = (await dp.Storage.getStorageKeyForFrame({frameId: frameId})).result.storageKey;

  testRunner.log("Get storage key by frame");
  testRunner.log(`storageKey obtained: ${!!key}`)
  testRunner.log(`storageKey in event  ${(key === storageId.storageKey) ? 'equal' : 'not equal'} to storage key by frame\n`);

  const items = (await dp.DOMStorage.getDOMStorageItems({storageId: storageId})).result.entries;

  testRunner.log("Get DOM storage items");
  testRunner.log(`items array ${items?.length ? 'not empty' : 'empty'}`);
  testRunner.log(`key: ${items[0][0]}, value: ${items[0][1]}`)

  testRunner.completeTest();
})
