async function waitForTool(name) {
  assert_implements(navigator.modelContextTesting,
      "modelContext is available");
  let tools = await navigator.modelContext.getTools();
  if (tools.some(t => t.name === name)) {
    return;
  }
  await new Promise(resolve => {
    const handler = async () => {
      let tools = await navigator.modelContext.getTools();
      if (tools.some(t => t.name === name)) {
        navigator.modelContext.removeEventListener('toolchange', handler);
        resolve();
      }
    };
    navigator.modelContext.addEventListener('toolchange', handler);
  });
}
