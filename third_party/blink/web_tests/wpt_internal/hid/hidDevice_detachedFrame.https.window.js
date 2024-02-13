// META: script=/resources/test-only-api.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;

async function getDetachedHidDevice() {
  let iframe = document.createElement('iframe');
  const ready =
      new Promise((resolve) => {window.addEventListener('message', e => {
                    if (e.data.type == 'Attach') {
                      fakeHidService.bind(e.data.handle);
                    } else if (e.data.type = 'Ready') {
                      resolve();
                    }
                  })})
  iframe.src = 'resources/iframe.html';
  document.body.appendChild(iframe);
  await ready;
  const iframeHid = iframe.contentWindow.navigator.hid;
  const guid = fakeHidService.addDevice(
      fakeHidService.makeDevice(kTestVendorId, kTestProductId));
  fakeHidService.setSelectedDevice(guid);
  const devices = await iframeHid.getDevices();
  assert_equals(devices.length, 1);
  // TODO(https://crbug.com/1290160): While it doesn't fail the test, the
  // console log showing error msg when iframe detached
  document.body.removeChild(iframe);
  // Set iframe to null to ensure that the GC cleans up as much as possible.
  iframe = null;
  GCController.collect();
  return devices[0];
}

hid_test(async (t) => {
  const device = await getDetachedHidDevice();
  try {
    await device.open();
    assert_unreached();
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'open() rejects in a detached context');

hid_test(async (t) => {
  const device = await getDetachedHidDevice();
  try {
    await device.forget();
    assert_unreached();
  } catch (e) {
    // Cannot use promise_rejects_dom() because |e| is thrown from a different
    // global.
    assert_equals(e.name, 'NotSupportedError');
  }
}, 'forget() rejects in a detached context');
