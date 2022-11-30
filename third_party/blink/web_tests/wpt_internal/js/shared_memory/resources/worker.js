importScripts('/resources/testharness.js');
onmessage = function (e) {
  let box = e.data;
  assert_equals(box.payload, 'hello from main');
  box.payload = 'hello from worker';
  postMessage('pong');
};
