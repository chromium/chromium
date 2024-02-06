// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: timeout=long

'use strict';

idl_test(
  ['../wpt_internal/system-wake-lock/resources/system-wake-lock'],
  ['dom', 'html'],
  async idl_array => {
    idl_array.add_objects({ Navigator: ['navigator'] });

    idl_array.add_objects({
      WakeLock: ['navigator.wakeLock'],
      WakeLockSentinel: ['sentinel'],
    });

    await test_driver.set_permission(
        { name: 'system-wake-lock' }, 'granted');
    self.sentinel = await navigator.wakeLock.request('system');
    self.sentinel.release();
  }
);
