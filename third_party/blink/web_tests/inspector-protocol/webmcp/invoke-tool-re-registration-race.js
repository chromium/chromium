(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
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
      'Tests that WebMCP.invokeTool targets tools by name, and therefore ' +
          'executes a different tool that was re-registered under the name of ' +
          'a tool the client previously observed.');

  // TODO(https://crbug.com/536063275): This documents the current, racy
  // behavior. Once CDP clients can target tools by ID, invoking the observed
  // (now unregistered) tool should fail instead of executing the replacement.

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
  await dp.Runtime.evaluate(
      {expression: 'window.swapTool()', awaitPromise: true});
  await removedPromise;
  const {params: {tools: [newTool]}} = await reAddedPromise;
  testRunner.log(`New tool: ${newTool.name} (${newTool.description})`);

  testRunner.log('Invoking the observed tool by name...');
  const respondedPromise = dp.WebMCP.onceToolResponded();
  await dp.WebMCP.invokeTool({
    frameId: observedTool.frameId,
    toolName: observedTool.name,
    input: {},
  });
  const {params: {status, output}} = await respondedPromise;
  testRunner.log(`toolResponded: status=${status}, output=${output}`);

  const {result: {result: {value: executed}}} =
      await dp.Runtime.evaluate({expression: 'window.executed.join(",")'});
  testRunner.log(`Executed tools: ${executed}`);

  testRunner.completeTest();
});
