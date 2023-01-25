'use strict';

let iframe = document.createElement('iframe');

promise_test(async () => {
  await new Promise(resolve => {
    iframe.src = '../resources/open-in-iframe.html';
    iframe.sandbox.add('allow-scripts');
    iframe.allow = 'hid';
    document.body.appendChild(iframe);
    iframe.addEventListener('load', resolve);
  });

  await new Promise(resolve => {
    iframe.contentWindow.postMessage({type: 'GetDevices'}, '*');

    window.addEventListener('message', (messageEvent) => {
      assert_equals(messageEvent.data, 'Success');
      resolve();
    });
  });
}, 'Calls to HID APIs from a sandboxed iframe are valid.');