(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <script>
        async function longTask(obj) {
            return new Promise((resolve, reject) => {
                // Do not resolve.
            });
        }
        navigator.modelContext.registerTool({
          execute: longTask,
          name: "test_tool",
          description: "A test WebMCP tool"
        });
      </script>
      `,
      'Tests WebMCP cancelInvocation command.');

  testRunner.log('Enabling WebMCP Domain');
  await dp.WebMCP.enable();

  const getFrameId = async () => {
    const {result} = await dp.Page.getFrameTree();
    return result.frameTree.frame.id;
  };

  const frameId = await getFrameId();

  testRunner.log('--- Invoking test_tool ---');
  let eventPromise = dp.WebMCP.onceToolResponded();
  let response = await dp.WebMCP.invokeTool({
      frameId,
      toolName: "test_tool",
      input: { }
  });

  testRunner.log(response.result, "invokeTool response: ", [], ["invocationId"]);

  const invocationId = response.result.invocationId;

  testRunner.log('--- Canceling test_tool with invalid ID ---');
  let invalidCancelResponse = await dp.WebMCP.cancelInvocation({
      invocationId: 'invalid-id'
  });
  testRunner.log(invalidCancelResponse.error.message, "invalid cancelInvocation response: ");

  testRunner.log('--- Canceling test_tool ---');
  let cancelResponse = await dp.WebMCP.cancelInvocation({
      invocationId
  });
  testRunner.log(cancelResponse.result, "cancelInvocation response: ");

  testRunner.log('--- Canceling test_tool again ---');
  let secondCancelResponse = await dp.WebMCP.cancelInvocation({
      invocationId
  });
  testRunner.log(secondCancelResponse.error.message, "second cancelInvocation response: ");

  let event = await eventPromise;
  let params = event.params;
  if (params.invocationId && params.invocationId === invocationId) {
      testRunner.log("toolResponded invocationId matches");
  }
  // Remove objectId to avoid flakiness from exception
  testRunner.log(params, "toolResponded event: ", [], ["invocationId", "objectId"]);

  testRunner.completeTest();
});