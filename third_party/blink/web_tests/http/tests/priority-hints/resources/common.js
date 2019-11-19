/**
 * These values map to some of the the current
 * priority enum members in blink::ResourceLoadPriority.
 * The values are exposed through window.internals
 * and in these tests, we use the below variables to represent
 * the exposed values in a readable way.
 */
const kLow = 1,
      kMedium = 2,
      kHigh = 3,
      kVeryHigh = 4;

// The UseCounter ID.
const kPriorityHints = 2738;

function assert_priority_onload(url, expected_priority, test) {
  // Set up priority promise synchronously.
  const priority_promise = getPriority(url);

  // The below function will run after the resource has been fetched. It will
  // assert that the priority promise we set-up earlier resolved to the correct
  // value.
  return () => {
    priority_promise.then(test.step_func((priority) => {
      assert_equals(priority, expected_priority);
      test.done();
    }));
  }
}

function getPriority(url) {
  return internals.getResourcePriority(url, document);
}

function clearUseCounter() {
  internals.clearUseCounter(document, kPriorityHints);
}
