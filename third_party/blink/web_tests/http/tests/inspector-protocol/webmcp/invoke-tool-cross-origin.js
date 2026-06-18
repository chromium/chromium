(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <form id="nav_form" toolname="navigate_tool" tooldescription="Navigates to another page" toolautosubmit action="http://devtools.oopif.test:8080/inspector-protocol/resources/empty.html">
        <input type="text" name="text">
      </form>

      `,
      'Tests WebMCP invokeTool command with cross-origin navigation.');



  testRunner.log('Enabling WebMCP Domain');
  await dp.WebMCP.enable();
  testRunner.log('Enabling Page Domain');
  await dp.Page.enable();

  const getFrameId = async () => {
    const {result} = await dp.Page.getFrameTree();
    return result.frameTree.frame.id;
  };

  const frameId = await getFrameId();

  testRunner.log('\n--- Invoking navigate_tool ---');
  // We expect a toolResponded event from the browser with an error.
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
