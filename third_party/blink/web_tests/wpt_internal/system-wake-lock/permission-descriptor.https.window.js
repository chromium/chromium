// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

promise_test(async t => {
  await test_driver.set_permission({name: 'system-wake-lock'}, 'denied');

  return navigator.permissions.query({name: 'system-wake-lock'})
      .then(status => {
        assert_class_string(status, 'PermissionStatus');
        assert_equals(status.state, 'denied');
      });
}, 'PermissionDescriptor with name=\'system-wake-lock\' works');
