(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(
      `
      <script>
        window.executed = [];
        window.controller = new AbortController();
        document.modelContext.registerTool({
          execute: () => {
            window.executed.push('original');
            return 'original';
          },
          name: 'swapped_tool',
          description: 'the original tool',
        }, {signal: window.controller.signal});

        window.swapTool = async function() {
          window.controller.abort();
          await document.modelContext.registerTool({
            execute: () => {
              window.executed.push('replacement');
              return 'replacement';
            },
            name: 'swapped_tool',
            description: 'a different tool that reused the name',
          });
        };
      </script>
      `,
      'Tests that WebMCP.invokeTool with the toolId of a tool the client ' +
          'previously observed fails once that tool is replaced by a different ' +
          'tool with the same name, while invoking by (deprecated) toolName ' +
          'executes the replacement.');

  testRunner.log('Enabling WebMCP Domain');
  const addedPromise = dp.WebMCP.onceToolsAdded();
  await dp.WebMCP.enable();
  const {params: {tools: [observedTool]}} = await addedPromise;
  testRunner.log(
      `Observed tool: ${observedTool.name} (${observedTool.description})`);

  testRunner.log('Unregistering it and registering a new tool with the same ' +
                 'name...');
  const removedPromise = dp.WebMCP.onceToolsRemoved();
  const reAddedPromise = dp.WebMCP.onceToolsAdded();
  await session.evaluateAsync('window.swapTool()');
  await removedPromise;
  const {params: {tools: [newTool]}} = await reAddedPromise;
  testRunner.log(`New tool: ${newTool.name} (${newTool.description})`);

  if (newTool.toolId !== observedTool.toolId) {
    testRunner.log('The new tool has a different toolId');
  }

  const logExecuted = async () => {
    const executed = await session.evaluate('window.executed.join(",")');
    testRunner.log(`Executed tools: [${executed}]`);
  };

  testRunner.log('Invoking the observed tool by toolId...');
  const {error} = await dp.WebMCP.invokeTool({
    frameId: observedTool.frameId,
    toolId: observedTool.toolId,
    input: {},
  });
  testRunner.log(`invokeTool error: ${error.message}`);
  await logExecuted();

  testRunner.log('Invoking the observed tool by (deprecated) toolName...');
  const respondedPromise = dp.WebMCP.onceToolResponded();
  await dp.WebMCP.invokeTool({
    frameId: observedTool.frameId,
    toolName: observedTool.name,
    input: {},
  });
  const {params: {status, output}} = await respondedPromise;
  testRunner.log(`toolResponded: status=${status}, output=${output}`);
  await logExecuted();

  testRunner.completeTest();
});
