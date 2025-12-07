(async function testRemoteObjects(testRunner) {
  const {dp, session} = await testRunner.startHTML(
      `<script>
         window.addEventListener("pagehide", () => history.pushState("", "", "#test"));
       </script>`,
      'Tests Page.reload together with a history navigation');
  await dp.Page.enable();

  await dp.Page.reload();

  testRunner.log('Waiting for navigation to be reported.');
  await dp.Page.onceFrameNavigated();
  testRunner.completeTest();
});
