'use strict';

const audioContext = new AudioContext();

promise_test(async t => {
  // Chromium enumerateDevices() returns 'default' as a deviceId for default
  // device. So, it should be accepted as a sinkId parameter.
  await audioContext.setSinkId('default');
  t.step(() => {
    // If 'default' is set, its sinkId should be 'default'.
    assert_equals(typeof audioContext.sinkId, 'string');
    assert_equals(audioContext.sinkId, 'default');
  });
  return audioContext.setSinkId('default');
}, "setSinkId with 'default' should be retrieved as 'default' in sinkId.");
