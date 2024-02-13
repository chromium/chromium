// META: script=/resources/test-only-api.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;
const kReportId = 0x01;
const kReportBytes = new Uint8Array([0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]);

function detachBuffer(buffer) {
  window.postMessage('', '*', [buffer]);
}

// Creates a HidDeviceInfo, adds it to the HID service, opens a connection to
// the newly created device, and returns the HidConnection fake.
async function addAndOpenDevice(fake) {
  const deviceInfo = fake.makeDevice(kTestVendorId, kTestProductId);
  const guid = deviceInfo.guid;
  const key = fake.addDevice(deviceInfo);
  fake.setSelectedDevice(key);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);

  await device.open();
  assert_true(device.opened);

  const fakeConnection = fake.getFakeConnection(guid);
  return {device, fakeConnection};
}

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Register an oninputreport listener and check that it receives a simulated
  // input report.
  fakeConnection.simulateInputReport(kReportId, kReportBytes);
  const inputReport = await oninputreport(device);
  assert_equals(inputReport.reportId, kReportId);
  compareDataViews(inputReport.data, new DataView(kReportBytes.buffer));
}, 'oninputreport event listener works');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate write() success.
  fakeConnection.queueExpectedWrite(true, kReportId, kReportBytes);
  await device.sendReport(kReportId, kReportBytes);
  fakeConnection.assertExpectationsMet();
}, 'sendReport works');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate write() failure.
  fakeConnection.queueExpectedWrite(false, kReportId, kReportBytes);
  await promise_rejects_dom(
      t, 'NotAllowedError', device.sendReport(kReportId, kReportBytes));
  fakeConnection.assertExpectationsMet();
}, 'Failed sendReport rejects');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  const promise = device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError', device.sendReport(kReportId, kReportBytes));
  await promise;
}, 'sendReport rejects while device is forgetting');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  await device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError', device.sendReport(kReportId, kReportBytes));
}, 'sendReport rejects when device is forgotten');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  const detachedReport = new Uint8Array(7);
  detachBuffer(detachedReport.buffer);
  const emptyReport = new Uint8Array(0);
  fakeConnection.queueExpectedWrite(true, kReportId, emptyReport);
  await device.sendReport(kReportId, detachedReport);
  fakeConnection.assertExpectationsMet();
}, 'sendReport treates a detached buffer as an empty report');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate getFeatureReport() success.
  fakeConnection.queueExpectedGetFeatureReport(true, kReportId, kReportBytes);
  const featureReport = await device.receiveFeatureReport(kReportId);
  compareDataViews(featureReport, new DataView(kReportBytes.buffer));
  fakeConnection.assertExpectationsMet();
}, 'receiveFeatureReport works');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate getFeatureReport() failure.
  fakeConnection.queueExpectedGetFeatureReport(false, kReportId, kReportBytes);
  await promise_rejects_dom(
      t, 'NotAllowedError', device.receiveFeatureReport(kReportId));
  fakeConnection.assertExpectationsMet();
}, 'Failed receiveFeatureReport rejects');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  const promise = device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError', device.receiveFeatureReport(kReportId));
  await promise;
}, 'receiveFeatureReport rejects while device is forgetting');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  await device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError', device.receiveFeatureReport(kReportId));
}, 'receiveFeatureReport rejects when device is forgotten');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate sendFeatureReport() success.
  fakeConnection.queueExpectedSendFeatureReport(true, kReportId, kReportBytes);
  await device.sendFeatureReport(kReportId, kReportBytes);
  fakeConnection.assertExpectationsMet();
}, 'sendFeatureReport works');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  // Simulate sendFeatureReport() failure.
  fakeConnection.queueExpectedSendFeatureReport(false, kReportId, kReportBytes);
  await promise_rejects_dom(
      t, 'NotAllowedError', device.sendFeatureReport(kReportId, kReportBytes));
  fakeConnection.assertExpectationsMet();
}, 'Failed sendFeatureReport rejects');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  const promise = device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError',
      device.sendFeatureReport(kReportId, kReportBytes));
  await promise;
}, 'sendFeatureReport rejects while device is forgetting');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  await device.forget();
  await promise_rejects_dom(
      t, 'InvalidStateError',
      device.sendFeatureReport(kReportId, kReportBytes));
}, 'sendFeatureReport rejects when device is forgotten');

hid_test(async (t, fake) => {
  const {device, fakeConnection} = await addAndOpenDevice(fake);
  const detachedReport = new Uint8Array(7);
  detachBuffer(detachedReport.buffer);
  const emptyReport = new Uint8Array(0);
  fakeConnection.queueExpectedSendFeatureReport(true, kReportId, emptyReport);
  await device.sendFeatureReport(kReportId, detachedReport);
  fakeConnection.assertExpectationsMet();
}, 'sendFeatureReport treates a detached buffer as an empty report');
