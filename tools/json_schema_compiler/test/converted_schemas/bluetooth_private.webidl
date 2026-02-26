// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[ExternalExtensionType="bluetooth.Device"]
typedef object BluetoothDevice;

[ExternalExtensionType="bluetooth.Transport"]
typedef object BluetoothTransport;

// Events that can occur during pairing. The method used for pairing varies
// depending on the capability of the two devices.
enum PairingEventType {
  // An alphanumeric PIN code is required to be entered by the user.
  "requestPincode",

  // Display a PIN code to the user.
  "displayPincode",

  // A numeric passkey is required to be entered by the user.
  "requestPasskey",

  // Display a zero padded 6 digit numeric passkey that the user entered on
  // the remote device. This event may occur multiple times during pairing to
  // update the entered passkey.
  "displayPasskey",

  // The number of keys inputted by the user on the remote device when
  // entering a passkey. This event may be called multiple times during
  // pairing to update the number of keys inputted.
  "keysEntered",

  // Requests that a 6 digit passkey be displayed and the user confirms that
  // both devies show the same passkey.
  "confirmPasskey",

  // Requests authorization for a pairing under the just-works model. It is up
  // to the app to ask for user confirmation.
  "requestAuthorization",

  // Pairing is completed.
  "complete"
};

// Results for connect(). See function declaration for details.
enum ConnectResultType {
  "alreadyConnected",
  "authCanceled",
  "authFailed",
  "authRejected",
  "authTimeout",
  "failed",
  "inProgress",
  "success",
  "unknownError",
  "unsupportedDevice",
  "notReady",
  "alreadyExists",
  "notConnected",
  "doesNotExist",
  "invalidArgs",
  "nonAuthTimeout",
  "noMemory",
  "jniEnvironment",
  "jniThreadAttach",
  "wakelock",
  "unexpectedState",
  "socketError"
};

// Valid pairing responses.
enum PairingResponse {
  "confirm",
  "reject",
  "cancel"
};

enum TransportType {
  "le",
  "bredr",
  "dual"
};

// A pairing event received from a Bluetooth device.
dictionary PairingEvent {
  required PairingEventType pairing;
  required BluetoothDevice device;
  DOMString pincode;
  long passkey;
  long enteredKey;
};

dictionary NewAdapterState {
  // The human-readable name of the adapter.
  DOMString name;

  // Whether or not the adapter has power.
  boolean powered;

  // Whether the adapter is discoverable by other devices.
  boolean discoverable;
};

dictionary SetPairingResponseOptions {
  // The remote device to send the pairing response.
  required BluetoothDevice device;

  // The response type.
  required PairingResponse response;

  // A 1-16 character alphanumeric set in response to
  // <code>requestPincode</code>.
  DOMString pincode;

  // An integer between 0-999999 set in response to
  // <code>requestPasskey</code>.
  long passkey;
};

dictionary DiscoveryFilter {
  // Transport type.
  TransportType transport;

  // uuid of service or array of uuids
  (DOMString or sequence<DOMString>) uuids;

  // RSSI ranging value. Only devices with RSSI higher than this value will be
  // reported.
  long rssi;

  // Pathloss ranging value. Only devices with pathloss lower than this value
  // will be reported.
  long pathloss;
};

// Listener callback for the onPairing event.
// |pairingEvent|: A pairing event.
callback OnPairingListener = undefined (PairingEvent pairingEvent);

interface OnPairingEvent : ExtensionEvent {
  static undefined addListener(OnPairingListener listener);
  static undefined removeListener(OnPairingListener listener);
  static boolean hasListener(OnPairingListener listener);
};

// Listener callback for the onDeviceAddressChanged event.
callback OnDeviceAddressChangedListener =
    undefined (BluetoothDevice device, DOMString oldAddress);

interface OnDeviceAddressChangedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceAddressChangedListener listener);
  static undefined removeListener(OnDeviceAddressChangedListener listener);
  static boolean hasListener(OnDeviceAddressChangedListener listener);
};

// Use the <code>chrome.bluetoothPrivate</code> API to control the Bluetooth
// adapter state and handle device pairing.
// NOTE: This IDL is dependent on bluetooth.idl.
[implemented_in="extensions/browser/api/bluetooth/bluetooth_private_api.h"]
interface BluetoothPrivate {
  // Changes the state of the Bluetooth adapter.
  // |adapterState|: The new state of the adapter.
  // |Returns|: Called when all the state changes have been completed.
  static Promise<undefined> setAdapterState(NewAdapterState adapterState);

  static Promise<undefined> setPairingResponse(
      SetPairingResponseOptions options);

  // Tears down all connections to the given device.
  static Promise<undefined> disconnectAll(DOMString deviceAddress);

  // Forgets the given device.
  static Promise<undefined> forgetDevice(DOMString deviceAddress);

  // Set or clear discovery filter.
  static Promise<undefined> setDiscoveryFilter(DiscoveryFilter discoveryFilter);

  // Connects to the given device. This will only throw an error if the
  // device address is invalid or the device is already connected. Otherwise
  // this will succeed and invoke |callback| with ConnectResultType.
  // |PromiseValue|: result
  static Promise<ConnectResultType> connect(DOMString deviceAddress);

  // Pairs the given device.
  static Promise<undefined> pair(DOMString deviceAddress);

  // Record that a pairing attempt finished. Ignores cancellations.
  static undefined recordPairing(BluetoothTransport transport,
                                 long pairingDurationMs,
                                 optional ConnectResultType result);

  // Record that a user-initiated reconnection attempt to an already paired
  // device finished. Ignores cancellations.
  static undefined recordReconnection(optional ConnectResultType result);

  // Record that a user selected a device to connect to.
  static undefined recordDeviceSelection(long selectionDurationMs,
                                         boolean wasPaired,
                                         BluetoothTransport transport);

  // Fired when a pairing event occurs.
  [maxListeners=1] static attribute OnPairingEvent onPairing;

  // Fired when a Bluetooth device changed its address.
  static attribute OnDeviceAddressChangedEvent onDeviceAddressChanged;
};

partial interface Browser {
  static attribute BluetoothPrivate bluetoothPrivate;
};
