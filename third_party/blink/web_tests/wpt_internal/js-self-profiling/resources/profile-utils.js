(function(global) {
  const TEST_SAMPLE_INTERVAL = 10;

  function forceSample() {
    window.internals.collectSample();
  }

  // Creates a new profile that captures the execution of when the given
  // function calls the `sample` function passed to it.
  async function profileFunction(func) {
    const profiler = await performance.profile({
      sampleInterval: TEST_SAMPLE_INTERVAL,
    });

    func(() => forceSample());

    const trace = await profiler.stop();

    // Sanity check ensuring that we captured a sample.
    assert_greater_than(trace.resources.length, 0);
    assert_greater_than(trace.frames.length, 0);
    assert_greater_than(trace.stacks.length, 0);
    assert_greater_than(trace.samples.length, 0);

    return trace;
  }

  async function testFunction(func, frame) {
    const trace = await profileFunction(func);
    assert_true(containsFrame(trace, frame), 'trace contains frame');
  }

  function substackMatches(trace, stackId, expectedStack) {
    if (expectedStack.length === 0) {
      return true;
    }
    if (stackId === undefined) {
      return false;
    }

    const stackElem = trace.stacks[stackId];
    const expectedFrame = expectedStack[0];

    if (!frameMatches(trace.frames[stackElem.frameId], expectedFrame)) {
      return false;
    }
    return substackMatches(trace, stackElem.parentId, expectedStack.slice(1));
  }

  // Returns true if the trace contains a frame matching the given specification.
  // We define a "match" as follows: a frame A matches an expectation E if (and
  // only if) for each field of E, A has the same value.
  function containsFrame(trace, expectedFrame) {
    return trace.frames.find(frame => {
      return frameMatches(frame, expectedFrame);
    }) !== undefined;
  }

  // Returns true if a trace contains a substack in one of its samples, ordered
  // leaf to root.
  function containsSubstack(trace, expectedStack) {
    return trace.samples.find(sample => {
      let stackId = sample.stackId;
      while (stackId !== undefined) {
        if (substackMatches(trace, stackId, expectedStack)) {
          return true;
        }
        stackId = trace.stacks[stackId].parentId;
      }
      return false;
    }) !== undefined;
  }

  function containsResource(trace, expectedResource) {
    return trace.resources.includes(expectedResource);
  }

  // Compares each set field of `expected` against the given frame `actual`.
  function frameMatches(actual, expected) {
    return (expected.name === undefined || expected.name === actual.name) &&
           (expected.resourceId === undefined || expected.resourceId === actual.resourceId) &&
           (expected.line === undefined || expected.line === actual.line) &&
           (expected.column === undefined || expected.column === actual.column);
  }

  global.ProfileUtils = {
    // Capturing
    profileFunction,
    forceSample,

    // Containment checks
    containsFrame,
    containsSubstack,
    containsResource,

    // Assertions
    testFunction,
  };
})(this);
