(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <form id="initial_declarative" toolautosubmit
            toolname="initial_declarative_tool"
            tooldescription="A declarative WebMCP tool"
            action="some_action.html">
        <input type="text" name="text">
      </form>

      <script>
        const inputSchema = {
          type: "object",
          properties: {
            "text": { type: "string" }
          },
          required: ["text"]
        };
        async function echo(obj) { return obj.text; }
        const initial_imperative_tool = {
          execute: echo,
          name: "initial_imperative_tool",
          description: "An imperative WebMCP tool",
          annotations: { readOnlyHint: true, untrustedContentHint: false, consequentialHint: false, debugging: false },
        };
        window.initialController = new AbortController();
        document.modelContext.registerTool(initial_imperative_tool, { signal: window.initialController.signal });

        window.registerNewTools = function() {
            window.sameNameController = new AbortController();
            document.modelContext.registerTool({
              execute: echo,
              name: "same_name_tool",
              description: "The first tool with this name",
            }, { signal: window.sameNameController.signal });
            document.modelContext.registerTool({
              execute: echo,
              name: "new_imperative_tool",
              description: "Another imperative tool",
              inputSchema,
              annotations: { readOnlyHint: false, untrustedContentHint: false, consequentialHint: false, debugging: false },
            });
            const form = document.createElement("form");
            form.id = "new_declarative";
            form.setAttribute("toolautosubmit", "");
            form.setAttribute("toolname", "new_declarative_tool");
            form.setAttribute("tooldescription", "Another declarative tool");
            form.setAttribute("action", "some_action.html");
            const input = document.createElement("input");
            input.type = "text";
            input.name = "text_name";
            form.appendChild(input);
            document.body.appendChild(form);
        };

        window.unregisterOneOfEach = function() {
            window.initialController.abort();
            const form = document.getElementById("initial_declarative");
            form.remove();
        };

        // Unregisters the first "same_name_tool", and registers a different
        // tool under the same name, which gets a new toolId.
        window.reRegisterSameName = function() {
            window.sameNameController.abort();
            document.modelContext.registerTool({
              execute: echo,
              name: "same_name_tool",
              description: "The second tool with this name",
            });
        };

        window.registerEvenMoreTools = function() {
            document.modelContext.registerTool({
              execute: echo,
              name: "newer_imperative_tool",
              description: "Another imperative tool",
              // no annotations
            });
        };
      </script>
      `,
      'Tests that WebMCP toolsAdded and toolsRemoved events fire correctly.');

  let addedCount = 0;
  let addedTarget = 0;
  let addedResolvers = null;
  dp.WebMCP.onToolsAdded(e => {
    testRunner.log(e.params, 'Adding ', undefined, ['toolId']);
    addedCount++;
    if (addedResolvers && addedCount >= addedTarget) {
      addedResolvers.resolve();
      addedResolvers = null;
    }
  });

  let removedCount = 0;
  let removedTarget = 0;
  let removedResolvers = null;
  dp.WebMCP.onToolsRemoved(e => {
    testRunner.log(e.params, 'Removing ', undefined, ['toolId']);
    removedCount++;
    if (removedResolvers && removedCount >= removedTarget) {
      removedResolvers.resolve();
      removedResolvers = null;
    }
  });

  async function waitAdded(count) {
    addedTarget += count;
    if (addedCount >= addedTarget)
      return;
    addedResolvers = Promise.withResolvers();
    return addedResolvers.promise;
  }

  async function waitRemoved(count) {
    removedTarget += count;
    if (removedCount >= removedTarget)
      return;
    removedResolvers = Promise.withResolvers();
    return removedResolvers.promise;
  }

  testRunner.log('Enabling WebMCP Domain');
  let enablePromise = waitAdded(1);
  await dp.WebMCP.enable();
  await enablePromise;

  testRunner.log(
      'Registering new imperative, declarative, and same-name tools...');
  session.evaluate('window.registerNewTools()');
  await waitAdded(3);

  testRunner.log('Unregistering one of each...');
  session.evaluate('window.unregisterOneOfEach()');
  await waitRemoved(2);

  testRunner.log('Replacing a tool with a new tool of the same name...');
  session.evaluate('window.reRegisterSameName()');
  await Promise.all([waitRemoved(1), waitAdded(1)]);

  testRunner.log('Disabling WebMCP Domain...');
  await dp.WebMCP.disable();

  testRunner.log('Registering even more tools (should not be reported)...');
  await session.evaluate('window.registerEvenMoreTools()');

  testRunner.completeTest();
});
