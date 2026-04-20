(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
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
          annotations: { readOnlyHint: true },
        };
        window.initialController = new AbortController();
        navigator.modelContext.registerTool(initial_imperative_tool, { signal: window.initialController.signal });

        window.registerNewTools = function() {
            navigator.modelContext.registerTool({
              execute: echo,
              name: "new_imperative_tool",
              description: "Another imperative tool",
              inputSchema,
              annotations: { readOnlyHint: false },
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

        window.registerEvenMoreTools = function() {
            navigator.modelContext.registerTool({
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
    testRunner.log(e.params, 'Adding ');
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
    testRunner.log(e.params, 'Removing ');
    removedCount++;
    if (removedResolvers && removedCount >= removedTarget) {
      removedResolvers.resolve();
      removedResolvers = null;
    }
  });

  async function waitAdded(count) {
    addedTarget += count;
    if (addedCount >= addedTarget) return;
    addedResolvers = Promise.withResolvers();
    return addedResolvers.promise;
  }

  async function waitRemoved(count) {
    removedTarget += count;
    if (removedCount >= removedTarget) return;
    removedResolvers = Promise.withResolvers();
    return removedResolvers.promise;
  }

  testRunner.log('Enabling WebMCP Domain');
  let enablePromise = waitAdded(1);
  await dp.WebMCP.enable();
  await enablePromise;

  testRunner.log('Registering a new imperative and a new declarative tool...');
  let addPromise = waitAdded(3);
  let removePromise1 = waitRemoved(1);
  await dp.Runtime.evaluate({expression: 'window.registerNewTools()'});
  await addPromise;
  await removePromise1;

  testRunner.log('Unregistering one of each...');
  let removePromise = waitRemoved(2);
  await dp.Runtime.evaluate({expression: 'window.unregisterOneOfEach()'});
  await removePromise;

  testRunner.log('Disabling WebMCP Domain...');
  await dp.WebMCP.disable();

  testRunner.log('Registering even more tools (should not be reported)...');
  await dp.Runtime.evaluate({expression: 'window.registerEvenMoreTools()'});

  testRunner.completeTest();
});
