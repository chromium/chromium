// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

const test_desc =
    'ControlledFrame and related types are not defined outside of an ' +
    'Isolated Web App';

test(() => {
  assert_true(typeof ControlledFrame === 'undefined');
  // 'WebView' is not defined when 'ControlledFrame' is defined, but 'WebView'
  // is defined for some non-web contexts. We test it here out of caution to
  // ensure the element isn't accidentally exposed.
  assert_true(typeof WebView == 'undefined');
}, test_desc);
