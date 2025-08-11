// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

// Testing the impact of the `ReduceDeviceMemory` flag.
test(t => {
  internals.runtimeFlags.reduceDeviceMemoryEnabled = false;
  const disabled = navigator.deviceMemory;

  internals.runtimeFlags.reduceDeviceMemoryEnabled = true;
  assert_equals(8.0, navigator.deviceMemory);

  internals.runtimeFlags.reduceDeviceMemoryEnabled = false;
  assert_equals(disabled, navigator.deviceMemory);
}, "Navigator.deviceMemory === 8 when the flag is enabled.");
