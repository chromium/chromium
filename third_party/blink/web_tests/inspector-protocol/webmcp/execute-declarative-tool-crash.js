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
        window.executeDeclarative = async function() {
          await navigator.modelContextTesting.executeTool(
            "declarative_tool", JSON.stringify({text: "hello"}));
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
