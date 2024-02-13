// META: script=/resources/test-only-api.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

async function getDetachedSerialPort() {
  let iframe = document.createElement('iframe');
  const ready = new Promise((resolve) => {
    window.addEventListener('message', e => {
      if (e.data.type == 'Attach') {
        fakeSerialService.bind(e.data.handle);
      } else if (e.data.type = 'Ready') {
        resolve();
      }
    })
  })

  iframe.src = 'resources/iframe.html';
  document.body.appendChild(iframe);
  await ready;

  const iframeSerial = iframe.contentWindow.navigator.serial;
  fakeSerialService.addPort();
  const ports = await iframeSerial.getPorts();
  assert_equals(ports.length, 1);

  document.body.removeChild(iframe);
  // Set iframe to null to ensure that the GC cleans up as much as possible.
  iframe = null;
  GCController.collect();

  return ports[0];
}

serial_test(async (t) => {
  const port = await getDetachedSerialPort();

  try {
    await port.open({ baudRate: 9600 });
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'open() rejects in a detached context');

serial_test(async (t) => {
  const port = await getDetachedSerialPort();

  try {
    await port.forget();
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'forget() rejects in a detached context');
