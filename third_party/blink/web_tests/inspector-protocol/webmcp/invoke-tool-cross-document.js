(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <form id="nav_form" toolname="navigate_tool" tooldescription="Navigates to another page" toolautosubmit>
        <input type="text" name="text">
      </form>
      <script>
        // No event.respondWith() here, so it will perform a real navigation.
      </script>
      `,
      'Tests WebMCP invokeTool command with cross-document navigation.');

  await dp.Runtime.evaluate({
    expression: `document.getElementById('nav_form').action = '${testRunner.url('resources/webmcp-response.html')}'`
  });

  testRunner.log('Enabling WebMCP Domain');
  await dp.WebMCP.enable();
  testRunner.log('Enabling Page Domain');
  await dp.Page.enable();

  const getFrameId = async () => {
    const {result} = await dp.Page.getFrameTree();
    return result.frameTree.frame.id;
  };

  const frameId = await getFrameId();

  testRunner.log('\\n--- Invoking navigate_tool ---');
  // We expect a toolResponded event from the new document.
  let eventPromise = dp.WebMCP.onceToolResponded();
  let response = await dp.WebMCP.invokeTool({
      frameId,
      toolName: "navigate_tool",
      input: { text: "unused" }
  });

  testRunner.log(response.result, "invokeTool response: ", [], ["invocationId"]);

  let event = await eventPromise;
  let params = event.params;
  if (params.invocationId && params.invocationId === response.result.invocationId) {
      testRunner.log("toolResponded invocationId matches");
  }
  testRunner.log(params, "toolResponded event: ", [], ["invocationId"]);

  testRunner.completeTest();
});
