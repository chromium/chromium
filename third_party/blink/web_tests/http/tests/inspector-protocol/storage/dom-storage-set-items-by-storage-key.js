(async function(testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests that settnig DOMStorage by storage-key works differently to doing so by origin\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  testRunner.log("Set storage item on an iframe of page with one origin:");
  await page.navigate('http://devtools-origin1.test:8000/inspector-protocol/resources/page-with-frame.html');
  testRunner.log("item set\n");

  testRunner.log("Read item from an iframe of page with other origin: ");
  await page.navigate('http://devtools-origin2.test:8000/inspector-protocol/resources/page-with-frame.html');
  testRunner.log(await session.evaluateAsync(`window.onMessagePromise`));

  testRunner.completeTest();
})
