(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const testUrl = 'resources/dom-get-document-test.html';
  const { dp } = await testRunner.startBlank('Tests DOM.documentUpdated event');

  await dp.DOM.enable();
  dp.Page.navigate({ url: testRunner.url(testUrl) });
  // Main frame event
  testRunner.log(await dp.DOM.onceDocumentUpdated());
  // iframe event
  testRunner.log(await dp.DOM.onceDocumentUpdated());

  testRunner.completeTest();
});
