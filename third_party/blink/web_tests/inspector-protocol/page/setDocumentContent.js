(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<div>Привет мир</div>`,
      'Tests that Page.setDocumentContent works.');

  await dp.Page.enable();
  testRunner.log('Page enabled');

  var resourceTreeResponse = await dp.Page.getResourceTree();
  var mainFrameId = resourceTreeResponse.result.frameTree.frame.id;
  testRunner.log('Main Frame obtained');

  testRunner.log('Document content before: ' + await session.evaluate(() => document.documentElement.outerHTML));
  await dp.Page.setDocumentContent({frameId: mainFrameId, html: '<div>こんにちは世界</div>'});
  testRunner.log('Document content set');
  testRunner.log('Document content after: ' + await session.evaluate(() => document.documentElement.outerHTML));

  testRunner.completeTest();
})
