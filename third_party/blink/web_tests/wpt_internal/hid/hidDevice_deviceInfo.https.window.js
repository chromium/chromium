// META: script=/resources/test-only-api.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;
const kTestReportId = 0x01;
const kButtonPrimary = 0x01;
const kTestGuid = 'test-guid';
const kTestProductName = 'test-device';
const kTestSerialNumber = 'test-serial';
const kTestDeviceNode = 'test-device-node';

// Constructs and returns a device.mojom.HidDeviceInfo representing a
// gamepad-like device with one input report. The input report has a single
// 8-bit field with a button usage.
async function createDeviceWithInputReport(fake) {
  const {
    GENERIC_DESKTOP_GAME_PAD,
    HID_COLLECTION_TYPE_APPLICATION,
    HidBusType,
    HidCollectionInfo,
    HidReportDescription,
    HidReportItem,
    HidUsageAndPage,
    PAGE_BUTTON,
    PAGE_GENERIC_DESKTOP
  } = await import('/gen/services/device/public/mojom/hid.mojom.m.js');
  const nullUsage = new HidUsageAndPage();
  const buttonUsage = new HidUsageAndPage();
  buttonUsage.usagePage = PAGE_BUTTON;
  buttonUsage.usage = kButtonPrimary;
  const gamePadUsage = new HidUsageAndPage();
  gamePadUsage.usagePage = PAGE_GENERIC_DESKTOP;
  gamePadUsage.usage = GENERIC_DESKTOP_GAME_PAD;

  const reportItem = new HidReportItem();
  reportItem.isRange = false;
  reportItem.isConstant = false;        // Data.
  reportItem.isVariable = true;         // Variable.
  reportItem.isRelative = false;        // Absolute.
  reportItem.wrap = false;              // No wrap.
  reportItem.isNonLinear = false;       // Linear.
  reportItem.noPreferredState = false;  // Preferred State.
  reportItem.hasNullPosition = false;   // No Null position.
  reportItem.isVolatile = false;        // Non Volatile.
  reportItem.isBufferedBytes = false;   // Bit Field.
  reportItem.usages = [buttonUsage];
  reportItem.usageMinimum = new HidUsageAndPage();
  reportItem.usageMaximum = new HidUsageAndPage();
  reportItem.designatorMinimum = 0;
  reportItem.designatorMaximum = 0;
  reportItem.stringMinimum = 0;
  reportItem.stringMaximum = 0;
  reportItem.logicalMinimum = 0;
  reportItem.logicalMaximum = 1;
  reportItem.physicalMinimum = 0;
  reportItem.physicalMaximum = 1;
  reportItem.unitExponent = 0;
  reportItem.unit = 0;        // Unitless.
  reportItem.reportSize = 8;  // 1 byte.
  reportItem.reportCount = 1;

  const report = new HidReportDescription();
  report.reportId = kTestReportId;
  report.items = [reportItem];

  const collection = new HidCollectionInfo();
  collection.usage = gamePadUsage;
  collection.reportIds = [kTestReportId];
  collection.collectionType = HID_COLLECTION_TYPE_APPLICATION;
  collection.inputReports = [report];
  collection.outputReports = [];
  collection.featureReports = [];
  collection.children = [];

  const deviceInfo = fake.makeDevice(kTestVendorId, kTestProductId);
  // The device guid will be set by fake.addDevice().
  deviceInfo.productName = kTestProductName;
  deviceInfo.serialNumber = kTestSerialNumber;
  deviceInfo.busType = HidBusType.HID_BUS_TYPE_USB;
  deviceInfo.reportDescriptor = [];
  deviceInfo.collections = [collection];
  deviceInfo.hasReportId = true;
  deviceInfo.maxInputReportSize = 1;
  deviceInfo.maxOutputReportSize = 0;
  deviceInfo.maxFeatureReportSize = 0;
  deviceInfo.deviceNode = kTestDeviceNode;

  return deviceInfo;
}

hid_test(async (t, fake) => {
  const {
    GENERIC_DESKTOP_GAME_PAD,
    HID_COLLECTION_TYPE_APPLICATION,
    HidBusType,
    HidCollectionInfo,
    HidReportDescription,
    HidReportItem,
    HidUsageAndPage,
    PAGE_BUTTON,
    PAGE_GENERIC_DESKTOP
  } = await import('/gen/services/device/public/mojom/hid.mojom.m.js');
  const deviceInfo = await createDeviceWithInputReport(fake);
  const guid = fake.addDevice(deviceInfo);
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array, 'devices instanceof Array');
  assert_equals(devices.length, 1, 'devices.length');

  const d = devices[0];
  assert_true(d instanceof HIDDevice, 'device instanceof HIDDevice');
  assert_false(d.opened, 'device.opened');
  assert_equals(d.vendorId, kTestVendorId, 'device.vendorId');
  assert_equals(d.productId, kTestProductId, 'device.productId');
  assert_equals(d.productName, kTestProductName, 'device.productName');
  assert_equals(d.collections.length, 1, 'device.collections.length');

  const c = d.collections[0];
  assert_equals(c.usagePage, PAGE_GENERIC_DESKTOP, 'collection.usagePage');
  assert_equals(c.usage, GENERIC_DESKTOP_GAME_PAD, 'collection.usage');
  assert_equals(c.type, HID_COLLECTION_TYPE_APPLICATION, 'collection.type');
  assert_equals(c.children.length, 0, 'collection.children.length');
  assert_equals(c.inputReports.length, 1, 'collection.inputReports.length');
  assert_equals(c.outputReports.length, 0, 'collection.outputReports.length');
  assert_equals(c.featureReports.length, 0, 'collection.featureReports.length');

  const r = c.inputReports[0];
  assert_equals(r.reportId, kTestReportId, 'report.reportId');
  assert_equals(r.items.length, 1, 'report.items.length');

  const i = r.items[0];
  assert_true(i.isAbsolute, 'reportItem.isAbsolute');
  assert_false(i.isArray, 'reportItem.isArray');
  assert_false(i.isBufferedBytes, 'reportItem.isBufferedBytes');
  assert_false(i.isConstant, 'reportItem.isConstant');
  assert_true(i.isLinear, 'reportItem.isLinear');
  assert_false(i.isRange, 'reportItem.isRange');
  assert_false(i.isVolatile, 'reportItem.isVolatile');
  assert_false(i.hasNull, 'reportItem.hasNull');
  assert_true(i.hasPreferredState, 'reportItem.hasPreferredState');
  assert_false(i.wrap, 'reportItem.wrap');
  assert_equals(i.usages.length, 1, 'reportItem.usages.length');
  assert_equals(i.usages[0], 0x00090001, 'reportItem.usages[0]');
  assert_equals(i.usageMinimum, undefined, 'reportItem.usageMinimum');
  assert_equals(i.usageMaximum, undefined, 'reportItem.usageMaximum');
  assert_equals(i.reportSize, 8, 'reportItem.reportSize');
  assert_equals(i.reportCount, 1, 'reportItem.reportCount');
  assert_equals(i.unitExponent, 0, 'reportItem.unitExponent');
  assert_equals(i.unitSystem, 'none', 'reportItem.unitSystem');
  assert_equals(
      i.unitFactorLengthExponent, 0, 'reportItem.unitFactorLengthExponent');
  assert_equals(
      i.unitFactorMassExponent, 0, 'reportItem.unitFactorMassExponent');
  assert_equals(
      i.unitFactorTimeExponent, 0, 'reportItem.unitFactorTimeExponent');
  assert_equals(
      i.unitFactorTemperatureExponent, 0,
      'reportItem.unitFactorTemperatureExponent');
  assert_equals(
      i.unitFactorCurrentExponent, 0, 'reportItem.unitFactorCurrentExponent');
  assert_equals(
      i.unitFactorLuminousIntensityExponent, 0,
      'reportItem.unitFactorLuminousIntensityExponent');
  assert_equals(i.logicalMinimum, 0, 'reportItem.logicalMinimum');
  assert_equals(i.logicalMaximum, 1, 'reportItem.logicalMaximum');
  assert_equals(i.physicalMinimum, 0, 'reportItem.physicalMinimum');
  assert_equals(i.physicalMaximum, 1, 'reportItem.physicalMaximum');
  // TODO(mattreynolds): Check i.strings.length.
}, 'HIDDevice preserves device info');

hid_test(async (t, fake) => {
  const deviceInfo = await createDeviceWithInputReport(fake);

  // Set the units to nano-Newtons. 10^-9 kg m/s^2 = 10^-4 g cm/s^2
  // |unit_exponent| is set to 0x0C which encodes the factor 10^-4.
  // |unit| is a coded value representing "SI Linear, g cm/s^2".
  // See "Device Class Definition for HID v1.11" section 6.2.2.7 for more
  // information about HID unit values.
  deviceInfo.collections[0].inputReports[0].items[0].unitExponent = 0x0C;
  deviceInfo.collections[0].inputReports[0].items[0].unit = 0x0000E111;

  const guid = fake.addDevice(deviceInfo);
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array, 'devices instanceof Array');
  assert_equals(devices.length, 1, 'devices.length');

  const d = devices[0];
  assert_true(d instanceof HIDDevice, 'device instanceof HIDDevice');
  assert_equals(d.collections.length, 1, 'device.collections.length');

  const c = d.collections[0];
  assert_equals(c.inputReports.length, 1, 'collection.inputReports.length');

  const r = c.inputReports[0];
  assert_equals(r.items.length, 1, 'report.items.length');

  const i = r.items[0];
  assert_equals(i.unitExponent, -4, 'reportItem.unitExponent');
  assert_equals(i.unitSystem, 'si-linear', 'reportItem.unitSystem');
  assert_equals(
      i.unitFactorLengthExponent, 1, 'reportItem.unitFactorLengthExponent');
  assert_equals(
      i.unitFactorMassExponent, 1, 'reportItem.unitFactorMassExponent');
  assert_equals(
      i.unitFactorTimeExponent, -2, 'reportItem.unitFactorTimeExponent');
  assert_equals(
      i.unitFactorTemperatureExponent, 0,
      'reportItem.unitFactorTemperatureExponent');
  assert_equals(
      i.unitFactorCurrentExponent, 0, 'reportItem.unitFactorCurrentExponent');
  assert_equals(
      i.unitFactorLuminousIntensityExponent, 0,
      'reportItem.unitFactorLuminousIntensityExponent');
}, 'HIDDevice preserves units');


hid_test(async (t, fake) => {
  const {
    PAGE_BUTTON,
  } = await import('/gen/services/device/public/mojom/hid.mojom.m.js');
  const deviceInfo = await createDeviceWithInputReport(fake);

  deviceInfo.collections[0].inputReports[0].items[0].isRange = true;
  deviceInfo.collections[0].inputReports[0].items[0].usages = [];
  deviceInfo.collections[0].inputReports[0].items[0].usageMinimum.usagePage =
      PAGE_BUTTON;
  deviceInfo.collections[0].inputReports[0].items[0].usageMinimum.usage = 1;
  deviceInfo.collections[0].inputReports[0].items[0].usageMaximum.usagePage =
      PAGE_BUTTON;
  deviceInfo.collections[0].inputReports[0].items[0].usageMaximum.usage = 8;

  const guid = fake.addDevice(deviceInfo);
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array, 'devices instanceof Array');
  assert_equals(devices.length, 1, 'devices.length');

  const d = devices[0];
  assert_true(d instanceof HIDDevice, 'device instanceof HIDDevice');
  assert_equals(d.collections.length, 1, 'device.collections.length');

  const c = d.collections[0];
  assert_equals(c.inputReports.length, 1, 'collection.inputReports.length');

  const r = c.inputReports[0];
  assert_equals(r.items.length, 1, 'report.items.length');

  const i = r.items[0];
  assert_true(i.isRange, 'reportItem.isRange');
  assert_equals(i.usages, undefined, 'reportItem.usages');
  assert_equals(i.usageMinimum, 0x00090001, 'reportItem.usageMinimum');
  assert_equals(i.usageMaximum, 0x00090008, 'reportItem.usageMaximum');
}, 'HIDDevice usage range item presents usageMinimum and usageMaximum');
