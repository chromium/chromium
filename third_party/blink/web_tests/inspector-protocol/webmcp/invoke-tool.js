(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <script>
        async function echo(obj) {
            return obj.text;
        }
        document.modelContext.registerTool({
          execute: echo,
          name: "test_tool",
          description: "A test WebMCP tool"
        });

        async function fail() {
            throw new Error("This tool always fails");
        }
        document.modelContext.registerTool({
            execute: fail,
            name: "failing_tool",
            description: "A failing WebMCP tool"
        });
      </script>
      `,
      'Tests WebMCP invokeTool command.');

  testRunner.log('Enabling WebMCP Domain');
  const addedPromise = dp.WebMCP.onceToolsAdded();
  await dp.WebMCP.enable();
  const {params: {tools}} = await addedPromise;
  const testTool = tools.find(tool => tool.name === 'test_tool');

  const getFrameId = async () => {
    const {result} = await dp.Page.getFrameTree();
    return result.frameTree.frame.id;
  };

  const frameId = await getFrameId();

  testRunner.log('\\n--- Invoking test_tool ---');
  let eventPromise = dp.WebMCP.onceToolResponded();
  let response = await dp.WebMCP.invokeTool({
      frameId,
      toolName: "test_tool",
      input: { text: "hello world" }
  });

  testRunner.log(response.result, "invokeTool response: ", [], ["invocationId"]);

  let event = await eventPromise;
  let params = event.params;
  if (params.invocationId && params.invocationId === response.result.invocationId) {
      testRunner.log("toolResponded invocationId matches");
  }
  testRunner.log(params, "toolResponded event: ", [], ["invocationId"]);

  testRunner.log('\\n--- Invoking test_tool by toolId ---');
  eventPromise = dp.WebMCP.onceToolResponded();
  response = await dp.WebMCP.invokeTool(
      {frameId, toolId: testTool.toolId, input: {text: 'hello by id'}});
  testRunner.log(response.result, 'invokeTool response: ', [],
                 ['invocationId']);
  event = await eventPromise;
  testRunner.log(event.params, 'toolResponded event: ', [], ['invocationId']);

  testRunner.log('\\n--- Invoking with both toolId and toolName ---');
  response = await dp.WebMCP.invokeTool({
    frameId,
    toolId: testTool.toolId,
    toolName: 'test_tool',
    input: {text: 'hello world'}
  });
  testRunner.log(response.error.message, 'invokeTool error message: ');

  testRunner.log('\\n--- Invoking with neither toolId nor toolName ---');
  response =
      await dp.WebMCP.invokeTool({frameId, input: {text: 'hello world'}});
  testRunner.log(response.error.message, 'invokeTool error message: ');

  testRunner.log('\\n--- Invoking with a malformed toolId ---');
  response = await dp.WebMCP.invokeTool(
      {frameId, toolId: 'not-a-tool-id', input: {text: 'hello world'}});
  testRunner.log(response.error.message, 'invokeTool error message: ');

  testRunner.log('\\n--- Invoking with invalid tool name ---');
  response = await dp.WebMCP.invokeTool({
      frameId,
      toolName: "nonexistent_tool",
      input: { text: "hello world" }
  });
  testRunner.log(response.error.message, "invokeTool error message: ");

  testRunner.log('\\n--- Invoking with invalid frameId ---');
  response = await dp.WebMCP.invokeTool({
      frameId: "invalid_frame_id",
      toolName: "test_tool",
      input: { text: "hello world" }
  });
  testRunner.log(response.error.message, "invokeTool error message: ");

  testRunner.log('\\n--- Invoking failing_tool (should fail execution) ---');
  eventPromise = dp.WebMCP.onceToolResponded();
  response = await dp.WebMCP.invokeTool({
      frameId,
      toolName: "failing_tool",
      input: {}
  });

  testRunner.log(response.result, "invokeTool response: ", [], ["invocationId"]);

  event = await eventPromise;
  let params2 = event.params;
  if (params2.invocationId && params2.invocationId === response.result.invocationId) {
      testRunner.log("toolResponded invocationId matches");
  }

  testRunner.log(params2, "toolResponded event for failing tool: ", [], ["invocationId", "objectId"]);

  testRunner.completeTest();
});
