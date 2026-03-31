(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <form id="my_form" toolname="declarative_tool" tooldescription="A declarative WebMCP tool" toolautosubmit>
        <input type="text" name="text">
      </form>
      <form id="array_form" toolname="declarative_array_tool" tooldescription="Returns an array" toolautosubmit>
        <input type="text" name="text">
      </form>
      <form id="string_form" toolname="declarative_string_tool" tooldescription="Returns a string" toolautosubmit>
        <input type="text" name="text">
      </form>
      <form id="number_form" toolname="declarative_number_tool" tooldescription="Returns a number" toolautosubmit>
        <input type="text" name="text">
      </form>

      <script>
        my_form.onsubmit = event => {
          event.preventDefault();
          event.respondWith({declarative: "success"});
        };
        array_form.onsubmit = event => {
          event.preventDefault();
          event.respondWith(["array", "result"]);
        };
        string_form.onsubmit = event => {
          event.preventDefault();
          event.respondWith("string result");
        };
        number_form.onsubmit = event => {
          event.preventDefault();
          event.respondWith(42);
        };
        const inputSchema = {
          type: "object",
          properties: {
            "text": { type: "string" }
          },
          required: ["text"]
        };
        async function imperative(obj) { return {text_was: obj.text}; }
        navigator.modelContext.registerTool({
          execute: imperative,
          name: "imperative_tool",
          description: "An imperative WebMCP tool",
          inputSchema
        });

        async function failing_js(obj) { throw new Error("JS Error"); }
        navigator.modelContext.registerTool({
          execute: failing_js,
          name: "failing_js_tool",
          description: "Fails in JS",
          inputSchema
        });

        async function abortable_tool(obj) {
          return new Promise((resolve, reject) => {
            // Wait for a long time, so it can be aborted
            setTimeout(() => resolve({text_was: obj.text}), 10000);
          });
        }
        navigator.modelContext.registerTool({
          execute: abortable_tool,
          name: "abortable_tool",
          description: "Aborts in JS",
          inputSchema
        });

        window.executeImperative = async function() {
          await navigator.modelContextTesting.executeTool("imperative_tool", JSON.stringify({text: "hello"}));
        };

        window.executeDeclarative = async function() {
          await navigator.modelContextTesting.executeTool("declarative_tool", JSON.stringify({text: "hello"}));
        };

        window.executeDeclarativeArray = async function() {
          const result = await navigator.modelContextTesting.executeTool("declarative_array_tool", JSON.stringify({text: "hello"}));
          return JSON.stringify(result);
        };

        window.executeDeclarativeString = async function() {
          const result = await navigator.modelContextTesting.executeTool("declarative_string_tool", JSON.stringify({text: "hello"}));
          return JSON.stringify(result);
        };

        window.executeDeclarativeNumber = async function() {
          const result = await navigator.modelContextTesting.executeTool("declarative_number_tool", JSON.stringify({text: "hello"}));
          return JSON.stringify(result);
        };

        window.executeFailingJS = async function() {
          try {
            await navigator.modelContextTesting.executeTool("failing_js_tool", JSON.stringify({text: "hello"}));
          } catch(e) {}
        };

        window.executeFailingModelContext = async function() {
          try {
            await navigator.modelContextTesting.executeTool("imperative_tool", "invalid json");
          } catch(e) {}
        };

        window.executeCancelled = async function() {
          try {
            const controller = new AbortController();
            const promise = navigator.modelContextTesting.executeTool("abortable_tool", JSON.stringify({text: "hello"}), {signal: controller.signal});
            controller.abort();
            await promise;
          } catch(e) {}
        };
      </script>
      `,
      'Tests that WebMCP toolInvoked and toolResponded events fire correctly.');

  dp.WebMCP.onToolInvoked(e => testRunner.log(e.params, 'Invoked ', undefined, ['invocationId']));
  dp.WebMCP.onToolResponded(e => testRunner.log(e.params, 'Responded ', undefined, ['invocationId']));

  testRunner.log('Enabling WebMCP Domain');
  await dp.WebMCP.enable();

  testRunner.log('Executing imperative tool...');
  let responded1 = dp.WebMCP.onceToolResponded();
  await dp.Runtime.evaluate({expression: 'window.executeImperative()', awaitPromise: true});
  await responded1;

  testRunner.log('Executing declarative tool...');
  let responded2 = dp.WebMCP.onceToolResponded();
  await dp.Runtime.evaluate({expression: 'window.executeDeclarative()', awaitPromise: true});
  await responded2;

  testRunner.log('Executing declarative array tool...');
  let respondedArray = dp.WebMCP.onceToolResponded();
  let arrayResult = await dp.Runtime.evaluate({expression: 'window.executeDeclarativeArray()', awaitPromise: true});
  testRunner.log('Array Result from executeTool:');
  testRunner.log(arrayResult);
  await respondedArray;

  testRunner.log('Executing declarative string tool...');
  let respondedString = dp.WebMCP.onceToolResponded();
  let stringResult = await dp.Runtime.evaluate({expression: 'window.executeDeclarativeString()', awaitPromise: true});
  testRunner.log('String Result from executeTool:');
  testRunner.log(stringResult);
  await respondedString;

  testRunner.log('Executing declarative number tool...');
  let respondedNumber = dp.WebMCP.onceToolResponded();
  let numberResult = await dp.Runtime.evaluate({expression: 'window.executeDeclarativeNumber()', awaitPromise: true});
  testRunner.log('Number Result from executeTool:');
  testRunner.log(numberResult);
  await respondedNumber;

  testRunner.log('Executing failing JS tool...');
  let responded3 = dp.WebMCP.onceToolResponded();
  await dp.Runtime.evaluate({expression: 'window.executeFailingJS()', awaitPromise: true});
  await responded3;

  testRunner.log('Executing failing ModelContext tool (invalid JSON)...');
  let responded4 = dp.WebMCP.onceToolResponded();
  await dp.Runtime.evaluate({expression: 'window.executeFailingModelContext()', awaitPromise: true});
  await responded4;

  testRunner.log('Executing cancelled tool...');
  let responded5 = dp.WebMCP.onceToolResponded();
  await dp.Runtime.evaluate({expression: 'window.executeCancelled()', awaitPromise: true});
  await responded5;

  testRunner.completeTest();
});
