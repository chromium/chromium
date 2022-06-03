'use strict';

importScripts("/resources/testharness.js");
importScripts("/resources/WebIDLParser.js", "/resources/idlharness.js");

idl_test(
  ['../../wpt_internal/system-wake-lock/resources/system-wake-lock'],
  ['dom', 'html'],
  async idl_array => {
    idl_array.add_objects({ WorkerNavigator: ['navigator'] });

    idl_array.add_objects({
      WakeLock: ['navigator.wakeLock'],
      WakeLockSentinel: ['sentinel'],
    });

    self.sentinel = await navigator.wakeLock.request('system');
    self.sentinel.release();
  }
);

done();