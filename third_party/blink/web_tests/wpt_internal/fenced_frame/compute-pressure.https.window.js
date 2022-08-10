// META: script=./resources/utils.js
// META: script=/common/utils.js
'use strict';

promise_test(async t => {
  const key = token();
  attachFencedFrame(generateURL(
      "resources/compute-pressure-inner.https.html",
      [key]));
  const result = await nextValueFromServer(key);
  assert_equals(result, 'observation failed');

}, 'PressureObserver.observe() fails in a fenced frame.');
