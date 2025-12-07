// Given a resource name, returns a promise that will resolve to the
// corresponding PerformanceResourceTiming entry. The promise will reject,
// however, if the PerformanceResourceTiming entry isn't observed within ~2
// seconds (scaled according to WPT timeout scaling).
const observe_entry = entry_name => {
  const entry = new Promise(resolve => {
    new PerformanceObserver((entry_list, observer) => {
      for (const entry of entry_list.getEntries()) {
        console.log(entry.name, entry.initiatorType);
        if (entry.name.includes(entry_name)) {
          resolve(entry);
          observer.disconnect();
          return;
        }
      }
    }).observe({"type": "resource", "buffered": true});
  });
  const timeout = new Promise((resolve, reject) => {
    step_timeout(() => {
      reject(new Error("observe_entry: timeout"));
    }, 2000);
  });
  // If the entry isn't observed within 2 seconds, assume it will never show
  // up.
  return Promise.race([entry, timeout]);
};

const initiator_type_test = (entry_name, expected_initiator, descriptor) => {
  promise_test(async () => {
    const entry = await observe_entry(entry_name);
    assert_equals(entry.initiatorType, expected_initiator);
  }, `The initiator type for ${descriptor} must be '${expected_initiator}'`);
};
