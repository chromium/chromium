// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Allocation authorities for Vendor IDs.
enum VendorIdSource {
  "bluetooth",
  "usb"
};

// Common device types recognized by Chrome.
enum DeviceType {
  "computer",
  "phone",
  "modem",
  "audio",
  "carAudio",
  "video",
  "peripheral",
  "joystick",
  "gamepad",
  "keyboard",
  "mouse",
  "tablet",
  "keyboardMouseCombo"
};

// Types for filtering bluetooth devices.
enum FilterType {
  "all",
  "known"
};

// Transport type of the bluetooth device.
enum Transport {
  "invalid",
  "classic",
  "le",
  "dual"
};

// Information about the state of the Bluetooth adapter.
dictionary AdapterState {
  // The address of the adapter, in the format 'XX:XX:XX:XX:XX:XX'.
  required DOMString address;

  // The human-readable name of the adapter.
  required DOMString name;

  // Indicates whether or not the adapter has power.
  required boolean powered;

  // Indicates whether or not the adapter is available (i.e. enabled).
  required boolean available;

  // Indicates whether or not the adapter is currently discovering.
  required boolean discovering;
};

// Information about the state of a known Bluetooth device. Note: this
// dictionary is also used in bluetooth_private.idl
dictionary Device {
  // The address of the device, in the format 'XX:XX:XX:XX:XX:XX'.
  required DOMString address;

  // The human-readable name of the device.
  DOMString name;

  // The class of the device, a bit-field defined by
  // http://www.bluetooth.org/en-us/specification/assigned-numbers/baseband.
  long deviceClass;

  // The Device ID record of the device, where available.
  VendorIdSource vendorIdSource;
  long vendorId;
  long productId;
  long deviceId;

  // The type of the device, if recognized by Chrome. This is obtained from
  // the |deviceClass| field and only represents a small fraction of the
  // possible device types. When in doubt you should use the |deviceClass|
  // field directly.
  DeviceType type;

  // Indicates whether or not the device is paired with the system.
  boolean paired;

  // Indicates whether the device is currently connected to the system.
  boolean connected;

  // Indicates whether the device is currently connecting to the system.
  boolean connecting;

  // Indicates whether the device is connectable.
  boolean connectable;

  // UUIDs of protocols, profiles and services advertised by the device.
  // For classic Bluetooth devices, this list is obtained from EIR data and
  // SDP tables. For Low Energy devices, this list is obtained from AD and
  // GATT primary services. For dual mode devices this may be obtained from
  // both.
  sequence<DOMString> uuids;

  // The received signal strength, in dBm. This field is avaliable and valid
  // only during discovery. Outside of discovery it's value is not specified.
  long inquiryRssi;

  // The transmitted power level. This field is avaliable only for LE devices
  // that include this field in AD. It is avaliable and valid only during
  // discovery.
  long inquiryTxPower;

  // The transport type of the bluetooth device.
  Transport transport;

  // The remaining battery of the device.
  long batteryPercentage;
};

dictionary BluetoothFilter {
  // Type of filter to apply to the device list. Default is all.
  FilterType filterType;

  // Maximum number of bluetooth devices to return. Default is 0 (no limit)
  // if unspecified.
  long limit;
};

// |state|: The new state of the adapter.
callback OnAdapterStateChangedListener = undefined(AdapterState state);

interface OnAdapterStateChangedEvent : ExtensionEvent {
  static undefined addListener(OnAdapterStateChangedListener listener);
  static undefined removeListener(OnAdapterStateChangedListener listener);
  static boolean hasListener(OnAdapterStateChangedListener listener);
};

callback OnDeviceAddedListener = undefined(Device device);

interface OnDeviceAddedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceAddedListener listener);
  static undefined removeListener(OnDeviceAddedListener listener);
  static boolean hasListener(OnDeviceAddedListener listener);
};

callback OnDeviceChangedListener = undefined(Device device);

interface OnDeviceChangedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceChangedListener listener);
  static undefined removeListener(OnDeviceChangedListener listener);
  static boolean hasListener(OnDeviceChangedListener listener);
};

callback OnDeviceRemovedListener = undefined(Device device);

interface OnDeviceRemovedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceRemovedListener listener);
  static undefined removeListener(OnDeviceRemovedListener listener);
  static boolean hasListener(OnDeviceRemovedListener listener);
};

// Use the <code>chrome.bluetooth</code> API to connect to a Bluetooth
// device. All functions report failures via chrome.runtime.lastError.
interface Bluetooth {
  // Get information about the Bluetooth adapter.
  // |Returns|: Called with an AdapterState object describing the adapter
  // state.
  // |PromiseValue|: adapterInfo: Object containing the adapter information.
  [requiredCallback] static Promise<AdapterState> getAdapterState();

  // Get information about a Bluetooth device known to the system.
  // |deviceAddress|: Address of device to get.
  // |Returns|: Called with the Device object describing the device.
  // |PromiseValue|: deviceInfo: Object containing the device information.
  [requiredCallback] static Promise<Device> getDevice(DOMString deviceAddress);

  // Get a list of Bluetooth devices known to the system, including paired
  // and recently discovered devices.
  // |filter|: Some criteria to filter the list of returned bluetooth devices.
  // If the filter is not set or set to <code>{}</code>, returned device list
  // will contain all bluetooth devices. Right now this is only supported in
  // ChromeOS, for other platforms, a full list is returned.
  // |Returns|: Called when the search is completed.
  // |PromiseValue|: deviceInfos: Array of object containing device information.
  [requiredCallback] static Promise<sequence<Device>> getDevices(
      optional BluetoothFilter filter);

  // Start discovery. Newly discovered devices will be returned via the
  // onDeviceAdded event. Previously discovered devices already known to
  // the adapter must be obtained using getDevices and will only be updated
  // using the |onDeviceChanged| event if information about them changes.
  //
  // Discovery will fail to start if this application has already called
  // startDiscovery.  Discovery can be resource intensive: stopDiscovery
  // should be called as soon as possible.
  // |Returns|: Called to indicate success or failure.
  static Promise<undefined> startDiscovery();

  // Stop discovery.
  // |Returns|: Called to indicate success or failure.
  static Promise<undefined> stopDiscovery();

  // Fired when the state of the Bluetooth adapter changes.
  static attribute OnAdapterStateChangedEvent onAdapterStateChanged;

  // Fired when information about a new Bluetooth device is available.
  static attribute OnDeviceAddedEvent onDeviceAdded;

  // Fired when information about a known Bluetooth device has changed.
  static attribute OnDeviceChangedEvent onDeviceChanged;

  // Fired when a Bluetooth device that was previously discovered has been
  // out of range for long enough to be considered unavailable again, and
  // when a paired device is removed.
  static attribute OnDeviceRemovedEvent onDeviceRemoved;
};

partial interface Browser {
  static attribute Bluetooth bluetooth;
};
