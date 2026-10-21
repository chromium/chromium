(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `<script>(${String(() => {
        function registerTool(name, description) {
          document.modelContext.registerTool({
            name,
            description,
            inputSchema: {description},
            execute: async () => ({result: description}),
          });
        }
        registerTool('ascii_tool', '30 x 40 cm');
        registerTool('latin1_times_tool', '30 \u00d7 40 cm');
        registerTool('latin1_degree_tool', 'Angle (\u00b0)');
        registerTool('latin1_utf8_lookalike_tool', '\u00c3\u00a9');
        registerTool('above_latin1_tool', '\u201c30 x 40 cm\u201d');
      })})()</script>`,
      'Tests that WebMCP toolsAdded inputSchema and toolResponded output preserve Latin-1 (U+0080-U+00FF) characters.');

  const toolsAddedPromise = dp.WebMCP.onceToolsAdded();
  await dp.WebMCP.enable();
  const {params: toolsAdded} = await toolsAddedPromise;

  for (const {frameId, name: toolName, inputSchema} of toolsAdded.tools) {
    testRunner.log(`toolsAdded (${toolName}): ${inputSchema?.description}`);
    const respondedPromise = dp.WebMCP.onceToolResponded();
    await dp.WebMCP.invokeTool({frameId, toolName, input: {}});
    const {params: responded} = await respondedPromise;
    testRunner.log(`toolResponded (${toolName}): ${responded.output.result}`);
  }

  testRunner.completeTest();
});
