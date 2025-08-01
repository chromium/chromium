// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

// Testing the impact of the `ReduceHardwareConcurrency` flag.
test(t => {
  internals.runtimeFlags.reduceHardwareConcurrencyEnabled = false;
  const disabled = navigator.hardwareConcurrency;

  internals.runtimeFlags.reduceHardwareConcurrencyEnabled = true;
  assert_equals(8, navigator.hardwareConcurrency);

  internals.runtimeFlags.reduceHardwareConcurrencyEnabled = false;
  assert_equals(disabled, navigator.hardwareConcurrency);
}, "Navigator.hardwareConcurrency === 8 when the flag is enabled.");
