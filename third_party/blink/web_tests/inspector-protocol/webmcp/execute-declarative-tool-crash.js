(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <form id="my_form"
            toolname="declarative_tool"
            tooldescription="A declarative WebMCP tool"
            action="some_action.html">
        <input type="text" name="text">
        <input type="submit">
      </form>

      <script>
        async function getTool(name) {
          let tools = await document.modelContext.getTools();
          let tool = tools.find(t => t.name === name);
          if (tool) return tool;
          return new Promise(resolve => {
            const handler = async () => {
              tools = await document.modelContext.getTools();
              tool = tools.find(t => t.name === name);
              if (tool) {
                document.modelContext.removeEventListener('toolchange', handler);
                resolve(tool);
              }
            };
            document.modelContext.addEventListener('toolchange', handler);
          });
        }
        window.executeDeclarative = async function() {
          const tool = await getTool("declarative_tool");
          await document.modelContext.executeTool(
            tool, JSON.stringify({text: "hello"}));
        };
      </script>
      `,
      'Tests that declarative tool execution does not crash.');

  await dp.WebMCP.enable();

  testRunner.log('Executing declarative tool...');
  const executePromise = dp.Runtime.evaluate(
      {expression: 'window.executeDeclarative()', awaitPromise: true});

  // Submit the form to unblock the execution.
  await dp.Runtime.evaluate(
      {expression: 'document.querySelector("input[type=submit]").click()'});

  await executePromise;

  testRunner.completeTest();
});
