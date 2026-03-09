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
        navigator.modelContext.registerTool({
          execute: echo,
          name: "initial_imperative_tool",
          description: "An imperative WebMCP tool",
        });

        window.registerNewTools = function() {
            navigator.modelContext.registerTool({
              execute: echo,
              name: "new_imperative_tool",
              description: "Another imperative tool",
              inputSchema,
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
            navigator.modelContext.unregisterTool("initial_imperative_tool");
            const form = document.getElementById("initial_declarative");
            form.remove();
        };
      </script>
      `,
      'Tests that WebMCP toolsAdded and toolsRemoved events fire correctly.');

  dp.WebMCP.onToolsAdded(e => testRunner.log(e.params, 'Adding '));
  dp.WebMCP.onToolsRemoved(e => testRunner.log(e.params, 'Removing '));

  testRunner.log('Enabling WebMCP Domain');
  await dp.WebMCP.enable();

  testRunner.log('Registering a new imperative and a new declarative tool...');
  await dp.Runtime.evaluate({expression: 'window.registerNewTools()'});

  testRunner.log('Unregistering one of each...');
  await dp.Runtime.evaluate({expression: 'window.unregisterOneOfEach()'});

  testRunner.completeTest();
});
