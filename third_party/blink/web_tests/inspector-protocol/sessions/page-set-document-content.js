(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that Page.setDocumentContent is observable from different session.');
  var page = await testRunner.createPage();

  var session1 = await page.createSession();
  session1.protocol.Page.enable();
  var session2 = await page.createSession();
  session2.protocol.Page.enable();

  var promise1 = session1.protocol.Page.onceFrameNavigated();
  var promise2 = session2.protocol.Page.onceFrameNavigated();
  testRunner.log('Reloading to grab frame ids');
  session1.protocol.Page.reload();

  var frameId1 = (await promise1).params.frame.id;
  var frameId2 = (await promise2).params.frame.id;

  testRunner.log('Setting document content in session1');
  await session1.protocol.Page.setDocumentContent({frameId: frameId1, html: '<div>Hello from session 1!</div>'});
  testRunner.log('Reading document content in session2:');
  testRunner.log(await session2.evaluate(`document.querySelector('div').textContent`));

  testRunner.completeTest();
})
