// This test ensures that tool registration can be aborted even if the
// AlgorithmHandle is garbage collected.
promise_test(async t => {
  const controller = new AbortController();
  const signal = controller.signal;

  navigator.modelContext.registerTool(
    {
      name: 'foo',
      description: 'bar',
      execute: () => {},
    },
    { signal }
  );

  // Trigger garbage collection.
  await gc({type: 'major', execution: 'async'});

  // Abort the signal.
  controller.abort();

  // Try to register the tool again. This should succeed because the old tool
  // should have been unregistered.
  try {
    navigator.modelContext.registerTool(
      {
        name: 'foo',
        description: 'bar',
        execute: () => {},
      },
      {}
    );
    assert_true(true, "Successfully registered tool again");
  } catch (e) {
    assert_unreached("Failed to register tool again: " + e.message);
  }
}, "Aborting tool registration after GC works");
