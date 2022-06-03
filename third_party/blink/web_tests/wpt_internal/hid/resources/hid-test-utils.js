import '/resources/testdriver.js';
import '/resources/testdriver-vendor.js';
import '/resources/testharness.js';
import '/resources/testharnessreport.js';

import {HidConnection, HidConnectionReceiver, HidDeviceInfo, HidManagerClientRemote} from '/gen/services/device/public/mojom/hid.mojom.m.js';
import {HidService, HidServiceReceiver} from '/gen/third_party/blink/public/mojom/hid/hid.mojom.m.js';

// Compare two DataViews byte-by-byte.
export function compareDataViews(actual, expected) {
  assert_true(actual instanceof DataView, 'actual is DataView');
  assert_true(expected instanceof DataView, 'expected is DataView');
  assert_equals(actual.byteLength, expected.byteLength, 'lengths equal');
  for (let i = 0; i < expected.byteLength; ++i) {
    assert_equals(actual.getUint8(i), expected.getUint8(i),
                  `Mismatch at byte ${i}.`);
  }
}

// Returns a Promise that resolves once |device| receives an input report.
export function oninputreport(device) {
  assert_true(device instanceof HIDDevice);
  return new Promise(resolve => { device.oninputreport = resolve; });
}

// Fake implementation of device.mojom.HidConnection. HidConnection represents
// an open connection to a HID device and can be used to send and receive
// reports.
class FakeHidConnection {
  constructor(client) {
    this.client_ = client;
    this.receiver_ = new HidConnectionReceiver(this);
    this.expectedWrites_ = [];
    this.expectedGetFeatureReports_ = [];
    this.expectedSendFeatureReports_ = [];
  }

  bindNewPipeAndPassRemote() {
    return this.receiver_.$.bindNewPipeAndPassRemote();
  }

  // Simulate an input report sent from the device to the host. The connection
  // client's onInputReport method will be called with the provided |reportId|
  // and |buffer|.
  simulateInputReport(reportId, reportData) {
    if (this.client_) {
      this.client_.onInputReport(reportId, reportData);
    }
  }

  // Specify the result for an expected call to write. If |success| is true the
  // write will be successful, otherwise it will simulate a failure. The
  // parameters of the next write call must match |reportId| and |buffer|.
  queueExpectedWrite(success, reportId, reportData) {
    this.expectedWrites_.push({
      params: {reportId, data: reportData},
      result: {success},
    });
  }

  // Specify the result for an expected call to getFeatureReport. If |success|
  // is true the operation is successful, otherwise it will simulate a failure.
  // The parameter of the next getFeatureReport call must match |reportId|.
  queueExpectedGetFeatureReport(success, reportId, reportData) {
    this.expectedGetFeatureReports_.push({
      params: {reportId},
      result: {success, buffer: reportData},
    });
  }

  // Specify the result for an expected call to sendFeatureReport. If |success|
  // is true the operation is successful, otherwise it will simulate a failure.
  // The parameters of the next sendFeatureReport call must match |reportId| and
  // |buffer|.
  queueExpectedSendFeatureReport(success, reportId, reportData) {
    this.expectedSendFeatureReports_.push({
      params: {reportId, data: reportData},
      result: {success},
    });
  }

  // Asserts that there are no more expected operations.
  assertExpectationsMet() {
    assert_equals(this.expectedWrites_.length, 0);
    assert_equals(this.expectedGetFeatureReports_.length, 0);
    assert_equals(this.expectedSendFeatureReports_.length, 0);
  }

  read() {}

  // Implementation of HidConnection::Write. Causes an assertion failure if
  // there are no expected write operations, or if the parameters do not match
  // the expected call.
  async write(reportId, buffer) {
    let expectedWrite = this.expectedWrites_.shift();
    assert_not_equals(expectedWrite, undefined);
    assert_equals(reportId, expectedWrite.params.reportId);
    let actual = new Uint8Array(buffer);
    compareDataViews(
        new DataView(actual.buffer, actual.byteOffset),
        new DataView(expectedWrite.params.data.buffer,
                     expectedWrite.params.data.byteOffset));
    return expectedWrite.result;
  }

  // Implementation of HidConnection::GetFeatureReport. Causes an assertion
  // failure if there are no expected write operations, or if the parameters do
  // not match the expected call.
  async getFeatureReport(reportId) {
    let expectedGetFeatureReport = this.expectedGetFeatureReports_.shift();
    assert_not_equals(expectedGetFeatureReport, undefined);
    assert_equals(reportId, expectedGetFeatureReport.params.reportId);
    return expectedGetFeatureReport.result;
  }

  // Implementation of HidConnection::SendFeatureReport. Causes an assertion
  // failure if there are no expected write operations, or if the parameters do
  // not match the expected call.
  async sendFeatureReport(reportId, buffer) {
    let expectedSendFeatureReport = this.expectedSendFeatureReports_.shift();
    assert_not_equals(expectedSendFeatureReport, undefined);
    assert_equals(reportId, expectedSendFeatureReport.params.reportId);
    let actual = new Uint8Array(buffer);
    compareDataViews(
        new DataView(actual.buffer, actual.byteOffset),
        new DataView(expectedSendFeatureReport.params.data.buffer,
                     expectedSendFeatureReport.params.data.byteOffset));
    return expectedSendFeatureReport.result;
  }
}


// A fake implementation of the HidService mojo interface. HidService manages
// HID device access for clients in the render process. Typically, when a client
// requests access to a HID device a chooser dialog is shown with a list of
// available HID devices. Selecting a device from the chooser also grants
// permission for the client to access that device.
//
// The fake implementation allows tests to simulate connected devices. It also
// skips the chooser dialog and instead allows tests to specify which device
// should be selected. All devices are treated as if the user had already
// granted permission.
class FakeHidService {
  constructor() {
    this.interceptor_ = new MojoInterfaceInterceptor(HidService.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new HidServiceReceiver(this);
    this.nextGuidValue_ = 0;
    this.simulateConnectFailure_ = false;
    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.devices_ = new Map();
    this.fakeConnections_ = new Map();
    this.selectedDevices_ = [];
  }

  // Creates and returns a HidDeviceInfo with the specified device IDs.
  makeDevice(vendorId, productId) {
    let guidValue = ++this.nextGuidValue_;
    let info = new HidDeviceInfo();
    info.guid = 'guid-' + guidValue.toString();
    info.physicalDeviceId = 'physical-device-id-' + guidValue.toString();
    info.vendorId = vendorId;
    info.productId = productId;
    info.productName = 'product name';
    info.serialNumber = '0';
    info.reportDescriptor = new Uint8Array();
    info.collections = [];
    info.deviceNode = 'device node';
    return info;
  }

  // Simulates a connected device. Returns the key used to store the device in
  // the map. The key is either the physical device ID, or the device GUID if it
  // has no physical device ID.
  addDevice(deviceInfo) {
    let key = deviceInfo.physicalDeviceId;
    if (key.length === 0)
      key = deviceInfo.guid;
    let devices = this.devices_.get(key);
    if (devices === undefined)
      devices = [];
    devices.push(deviceInfo);
    this.devices_.set(key, devices);
    if (this.client_)
      this.client_.deviceAdded(deviceInfo);
    return key;
  }

  // Simulates disconnecting a connected device.
  removeDevice(key) {
    let devices = this.devices_.get(key);
    this.devices_.delete(key);
    if (this.client_ && devices) {
      devices.forEach(deviceInfo => {
        this.client_.deviceRemoved(deviceInfo);
      });
    }
  }

  // Simulates updating the device information for a connected device.
  changeDevice(deviceInfo) {
    let key = deviceInfo.physicalDeviceId;
    if (key.length === 0)
      key = deviceInfo.guid;
    let devices = this.devices_.get(key);
    if (devices === undefined)
      devices = [];
    let i = devices.length;
    while (i--) {
      if (devices[i].guid == deviceInfo.guid)
        devices.splice(i, 1);
    }
    devices.push(deviceInfo);
    this.devices_.set(key, devices);
    if (this.client_)
      this.client_.deviceChanged(deviceInfo);
    return key;
  }

  // Sets a flag that causes the next call to connect() to fail.
  simulateConnectFailure() {
    this.simulateConnectFailure_ = true;
  }

  // Sets the key of the device that will be returned as the selected item the
  // next time requestDevice is called. The device with this key must have been
  // previously added with addDevice.
  setSelectedDevice(key) {
    this.selectedDevices_ = this.devices_.get(key);
  }

  // Returns the fake HidConnection object for this device, if there is one. A
  // connection is created once the device is opened.
  getFakeConnection(guid) {
    return this.fakeConnections_.get(guid);
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  registerClient(client) {
    this.client_ = client;
  }

  // Returns an array of connected devices. Normally this would only include
  // devices that the client has already been granted permission to access, but
  // for the fake implementation all simulated devices are returned.
  async getDevices() {
    let devices = [];
    this.devices_.forEach((value) => {
      devices = devices.concat(value);
    });
    return {devices};
  }

  // Simulates a device chooser prompt, returning |selectedDevices_| as the
  // simulated selection. |filters| is ignored.
  async requestDevice(filters) {
    return {devices: this.selectedDevices_};
  }

  // Returns a fake connection to the device with the specified GUID. If
  // |connectionClient| is not null, its onInputReport method will be called
  // when input reports are received. If simulateConnectFailure() was called
  // then a null connection is returned instead, indicating failure.
  async connect(guid, connectionClient) {
    if (this.simulateConnectFailure_) {
      this.simulateConnectFailure_ = false;
      return {connection: null};
    }
    const fakeConnection = new FakeHidConnection(connectionClient);
    this.fakeConnections_.set(guid, fakeConnection);
    return {connection: fakeConnection.bindNewPipeAndPassRemote()};
  }
}

let fakeHidService = new FakeHidService();

export function hid_test(func, name, properties) {
  promise_test(async (test) => {
    fakeHidService.start();
    try {
      await func(test, fakeHidService);
    } finally {
      fakeHidService.stop();
      fakeHidService.reset();
    }
  }, name, properties);
}

export function trustedClick() {
  return new Promise(resolve => {
    let button = document.createElement('button');
    button.textContent = 'click to continue test';
    button.style.display = 'block';
    button.style.fontSize = '20px';
    button.style.padding = '10px';
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
    test_driver.click(button);
  });
}
