// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Dialog used for pairing a provided |pairing-device|. Set |show-error| to
 * show the error results from a pairing event instead of the pairing UI.
 * NOTE: This module depends on I18nBehavior which depends on loadTimeData.
 */

const PairingEventType = chrome.bluetoothPrivate.PairingEventType;

Polymer({
  is: 'bluetooth-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for bluetooth calls. Set in bluetooth-page.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls.
     * @type {BluetoothPrivate}
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },

    noCancel: Boolean,

    dialogTitle: String,

    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     */
    pairingDevice: Object,

    /**
     * Current Pairing event.
     * @private {?chrome.bluetoothPrivate.PairingEvent}
     */
    pairingEvent_: {
      type: Object,
      value: null,
    },

    /**
     * May be set by the host to show a pairing error result, or may be
     * set by the dialog if a pairing or connect error occured.
     * @private
     */
    errorMessage_: String,

    /**
     * Pincode or passkey value, used to trigger connect enabled changes.
     * @private
     */
    pinOrPass_: String,

    /**
     * @const {!Array<number>}
     * @private
     */
    digits_: {
      type: Array,
      readOnly: true,
      value: [0, 1, 2, 3, 4, 5],
    },

    /**
     * The time in milliseconds at which a connection attempt started (that is,
     * when this dialog is opened).
     * @private {?number}
     */
    connectionAttemptStartTimestampMs_: {
      type: Number,
      value: null,
    },

    /**
     * The time in milliseconds at which the user is asked to comfirm the
     * pairing auth process.
     * @private {?number}
     */
    pairingUserAuthAttemptStartTimestampMs_: {
      type: Number,
      value: null,
    },

    /**
     * The time in milliseconds at which the user confirms the pairing auth
     * process.
     * @private {?number}
     */
    pairingUserAuthAttemptFinishTimestampMs_: {
      type: Number,
      value: null,
    },
  },

  observers: [
    'dialogUpdated_(errorMessage_, pairingEvent_)',
    'pairingChanged_(pairingDevice, pairingEvent_)',
  ],

  /**
   * Listener for chrome.bluetoothPrivate.onPairing events.
   * @private {?function(!chrome.bluetoothPrivate.PairingEvent)}
   */
  bluetoothPrivateOnPairingListener_: null,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceChanged events.
   * @private {?function(!chrome.bluetooth.Device)}
   */
  bluetoothDeviceChangedListener_: null,

  open: function() {
    this.startPairing();
    this.pinOrPass_ = '';
    this.getDialog_().showModal();
    this.itemWasFocused_ = false;
  },

  close: function() {
    this.endPairing();
    const dialog = this.getDialog_();
    if (dialog.open) {
      dialog.close();
    }
  },

  /**
   * Updates the dialog after a connection attempt.
   * @param {!chrome.bluetooth.Device} device The device connected to.
   * @param {!boolean} wasPairing True if the device required pairing before
   *     connecting.
   * @param {!{message: string}} lastError chrome.runtime.lastError.
   * @param {chrome.bluetoothPrivate.ConnectResultType} result The connect
   *     result.
   * @return {boolean} True if the dialog considers this a fatal error and
   *     is displaying an error message.
   */
  endConnectionAttempt: function(device, wasPairing, lastError, result) {
    if (wasPairing) {
      const transport = device.transport ? device.transport :
                                           chrome.bluetooth.Transport.INVALID;
      this.recordPairingMetrics_(transport, lastError, result);
    }

    let error;
    if (lastError) {
      error = lastError.message;
    } else {
      switch (result) {
        case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
        case chrome.bluetoothPrivate.ConnectResultType.ALREADY_CONNECTED:
        case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
        case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
          this.errorMessage_ = '';
          return false;
        default:
          error = result;
      }
    }

    // Attempting to connect and pair has failed. Remove listeners.
    this.endPairing();

    const name = device.name || device.address;
    let id = 'bluetooth_connect_' + error;
    if (!this.i18nExists(id)) {
      console.error(
          'Unexpected error connecting to:', name, 'error:', error,
          'result:', result);
      id = 'bluetooth_connect_failed';
    }
    this.errorMessage_ = this.i18n(id, name);

    return true;
  },

  /** @private */
  dialogUpdated_: function() {
    if (this.showEnterPincode_()) {
      this.$$('#pincode').focus();
    } else if (this.showEnterPasskey_()) {
      this.$$('#passkey').focus();
    } else if (this.showAcceptReject_()) {
      this.$$('#accept-button').focus();
    }
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_: function() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /** @private */
  onCancelTap_: function() {
    this.getDialog_().cancel();
  },

  /** @private */
  onDialogCanceled_: function() {
    if (!this.errorMessage_) {
      this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CANCEL);
    }
    this.endPairing();
  },

  /** Called when the dialog is opened. Starts listening for pairing events. */
  startPairing: function() {
    if (!this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivateOnPairingListener_ =
          this.onBluetoothPrivateOnPairing_.bind(this);
      this.bluetoothPrivate.onPairing.addListener(
          this.bluetoothPrivateOnPairingListener_);

      this.connectionAttemptStartTimestampMs_ = Date.now();
    }
    if (!this.bluetoothDeviceChangedListener_) {
      this.bluetoothDeviceChangedListener_ =
          this.onBluetoothDeviceChanged_.bind(this);
      this.bluetooth.onDeviceChanged.addListener(
          this.bluetoothDeviceChangedListener_);
    }
  },

  /** Called when the dialog is closed. */
  endPairing: function() {
    if (this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivate.onPairing.removeListener(
          this.bluetoothPrivateOnPairingListener_);
      this.bluetoothPrivateOnPairingListener_ = null;
    }
    if (this.bluetoothDeviceChangedListener_) {
      this.bluetooth.onDeviceChanged.removeListener(
          this.bluetoothDeviceChangedListener_);
      this.bluetoothDeviceChangedListener_ = null;
    }
    this.pairingEvent_ = null;
  },

  /**
   * Process bluetoothPrivate.onPairing events.
   * @param {!chrome.bluetoothPrivate.PairingEvent} event
   * @private
   */
  onBluetoothPrivateOnPairing_: function(event) {
    if (!this.pairingDevice ||
        event.device.address != this.pairingDevice.address) {
      return;
    }
    if (event.pairing == PairingEventType.KEYS_ENTERED &&
        event.passkey === undefined && this.pairingEvent_) {
      // 'keysEntered' event might not include the updated passkey so preserve
      // the current one.
      event.passkey = this.pairingEvent_.passkey;
    }
    this.pairingEvent_ = event;
  },

  /**
   * Process bluetooth.onDeviceChanged events. This ensures that the dialog
   * updates when the connection state changes.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceChanged_: function(device) {
    if (!this.pairingDevice || device.address != this.pairingDevice.address) {
      return;
    }
    this.pairingDevice = device;
  },

  /** @private */
  pairingChanged_: function() {
    if (this.pairingDevice === undefined) {
      return;
    }

    if (!this.pairingUserAuthAttemptStartTimestampMs_ && !!this.pairingEvent_ &&
        (this.pairingEvent_.pairing === PairingEventType.REQUEST_PINCODE ||
         this.pairingEvent_.pairing === PairingEventType.REQUEST_PASSKEY ||
         this.pairingEvent_.pairing === PairingEventType.DISPLAY_PINCODE ||
         this.pairingEvent_.pairing === PairingEventType.DISPLAY_PASSKEY ||
         this.pairingEvent_.pairing === PairingEventType.CONFIRM_PASSKEY ||
         this.pairingEvent_.pairing === PairingEventType.KEYS_ENTERED)) {
      this.pairingUserAuthAttemptStartTimestampMs_ = Date.now();
    }

    // Auto-close the dialog when pairing completes.
    if (this.pairingDevice.paired && !this.pairingDevice.connecting &&
        this.pairingDevice.connected) {
      if (this.pairingUserAuthAttemptStartTimestampMs_) {
        this.pairingUserAuthAttemptFinishTimestampMs_ = Date.now();
      }

      this.close();
      return;
    }
    this.errorMessage_ = '';
    this.pinOrPass_ = '';
  },

  /**
   * @return {string}
   * @private
   */
  getMessage_: function() {
    let message;
    if (!this.pairingEvent_) {
      message = 'bluetoothStartConnecting';
    } else {
      message = this.getEventDesc_(this.pairingEvent_.pairing);
    }

    let pairingDeviceName = '';
    if (this.pairingDevice && this.pairingDevice.name) {
      pairingDeviceName = this.pairingDevice.name;
    }

    return this.i18n(message, pairingDeviceName);
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPincode_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.REQUEST_PINCODE;
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPasskey_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDisplayPassOrPin_: function() {
    if (!this.pairingEvent_) {
      return false;
    }
    const pairing = this.pairingEvent_.pairing;
    return (
        pairing == PairingEventType.DISPLAY_PINCODE ||
        pairing == PairingEventType.DISPLAY_PASSKEY ||
        pairing == PairingEventType.CONFIRM_PASSKEY ||
        pairing == PairingEventType.KEYS_ENTERED);
  },

  /**
   * @return {boolean}
   * @private
   */
  showAcceptReject_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.CONFIRM_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showConnect_: function() {
    if (!this.pairingEvent_) {
      return false;
    }
    const pairing = this.pairingEvent_.pairing;
    return pairing == PairingEventType.REQUEST_PINCODE ||
        pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  enableConnect_: function() {
    if (!this.showConnect_()) {
      return false;
    }
    const inputId =
        (this.pairingEvent_.pairing == PairingEventType.REQUEST_PINCODE) ?
        '#pincode' :
        '#passkey';
    const crInput = /** @type {!CrInputElement} */ (this.$$(inputId));
    assert(crInput);
    /** @type {string} */ const value = crInput.value;
    return !!value && crInput.validate();
  },

  /**
   * @return {boolean}
   * @private
   */
  showDismiss_: function() {
    return (!!this.pairingDevice && this.pairingDevice.paired) ||
        (!!this.pairingEvent_ &&
         this.pairingEvent_.pairing == PairingEventType.COMPLETE);
  },

  /** @private */
  onAcceptTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onConnectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onRejectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.REJECT);
  },

  /**
   * @param {!chrome.bluetoothPrivate.PairingResponse} response
   * @private
   */
  sendResponse_: function(response) {
    if (!this.pairingDevice) {
      return;
    }
    const options =
        /** @type {!chrome.bluetoothPrivate.SetPairingResponseOptions} */ (
            {device: this.pairingDevice, response: response});
    if (response == chrome.bluetoothPrivate.PairingResponse.CONFIRM) {
      const pairing = this.pairingEvent_.pairing;
      if (pairing == PairingEventType.REQUEST_PINCODE) {
        options.pincode = this.$$('#pincode').value;
      } else if (pairing == PairingEventType.REQUEST_PASSKEY) {
        options.passkey = parseInt(this.$$('#passkey').value, 10);
      }
    }
    this.bluetoothPrivate.setPairingResponse(options, () => {
      if (chrome.runtime.lastError) {
        // TODO(stevenjb): Show error.
        console.error(
            'Error setting pairing response: ' + options.device.name +
            ': Response: ' + options.response +
            ': Error: ' + chrome.runtime.lastError.message);
      }
      this.close();
    });

    this.fire('response', options);
  },

  /**
   * @param {!PairingEventType} eventType
   * @return {string}
   * @private
   */
  getEventDesc_: function(eventType) {
    assert(eventType);
    if (eventType == PairingEventType.COMPLETE ||
        eventType == PairingEventType.REQUEST_AUTHORIZATION) {
      return 'bluetoothStartConnecting';
    }
    return 'bluetooth_' + /** @type {string} */ (eventType);
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinDigit_: function(index) {
    if (!this.pairingEvent_) {
      return '';
    }
    let digit = '0';
    const pairing = this.pairingEvent_.pairing;
    if (pairing == PairingEventType.DISPLAY_PINCODE &&
        this.pairingEvent_.pincode &&
        index < this.pairingEvent_.pincode.length) {
      digit = this.pairingEvent_.pincode[index];
    } else if (
        this.pairingEvent_.passkey &&
        (pairing == PairingEventType.DISPLAY_PASSKEY ||
         pairing == PairingEventType.KEYS_ENTERED ||
         pairing == PairingEventType.CONFIRM_PASSKEY)) {
      const passkeyString =
          String(this.pairingEvent_.passkey).padStart(this.digits_.length, '0');
      digit = passkeyString[index];
    }
    return digit;
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinClass_: function(index) {
    if (!this.pairingEvent_) {
      return '';
    }
    if (this.pairingEvent_.pairing == PairingEventType.CONFIRM_PASSKEY) {
      return 'confirm';
    }
    let cssClass = 'display';
    if (this.pairingEvent_.pairing == PairingEventType.DISPLAY_PASSKEY) {
      if (index == 0) {
        cssClass += ' next';
      } else {
        cssClass += ' untyped';
      }
    } else if (
        this.pairingEvent_.pairing == PairingEventType.KEYS_ENTERED &&
        this.pairingEvent_.enteredKey) {
      const enteredKey = this.pairingEvent_.enteredKey;  // 1-7
      const lastKey = this.digits_.length;               // 6
      if ((index == -1 && enteredKey > lastKey) || (index + 1 == enteredKey)) {
        cssClass += ' next';
      } else if (index > enteredKey) {
        cssClass += ' untyped';
      }
    }
    return cssClass;
  },

  /**
   * Record metrics for pairing attempt results.
   * @param {!chrome.bluetooth.Transport} transport The transport type of the
   *     device.
   * @param {!{message: string}} lastError chrome.runtime.lastError.
   * @param {!chrome.bluetoothPrivate.ConnectResultType} result The connection
   *     result.
   * @private
   */
  recordPairingMetrics_: function(transport, lastError, result) {
    // TODO(crbug.com/953149): Also create metrics which break down the simple
    // boolean success/failure metric with error reasons, including |lastError|.

    let success;
    if (lastError) {
      success = false;
    } else {
      switch (result) {
        case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
          success = true;
          break;
        case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
        case chrome.bluetoothPrivate.ConnectResultType.ALREADY_CONNECTED:
        case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
        case chrome.bluetoothPrivate.ConnectResultType.AUTH_REJECTED:
          // Cases like this do not reflect success or failure to pair,
          // particularly AUTH_CANCELED or AUTH_REJECTED. Do not emit to
          // metrics.
          return;
        default:
          success = false;
          break;
      }
    }

    chrome.bluetoothPrivate.recordPairing(
        success, transport, this.getPairingDurationMs_());
  },

  /**
   * Calculate how long it took to complete pairing, excluding how long the user
   * took to confirm the pairing auth process.
   * @return {number}
   * @private
   */
  getPairingDurationMs_: function() {
    let unadjustedPairingDurationMs = 0;
    if (this.connectionAttemptStartTimestampMs_) {
      unadjustedPairingDurationMs =
          Date.now() - this.connectionAttemptStartTimestampMs_;
    } else {
      console.error('No connection start timestamp present.');
    }

    let userAuthActionDurationMs = 0;
    if (this.pairingUserAuthAttemptStartTimestampMs_) {
      if (this.pairingUserAuthAttemptFinishTimestampMs_) {
        userAuthActionDurationMs =
            this.pairingUserAuthAttemptFinishTimestampMs_ -
            this.pairingUserAuthAttemptStartTimestampMs_;
      } else {
        console.error(
            'No auth attempt finish timestamp present to' +
            ' complement start timestamp.');
      }
    }

    this.connectionAttemptStartTimestampMs_ = null;
    this.pairingUserAuthAttemptStartTimestampMs_ = null;
    this.pairingUserAuthAttemptFinishTimestampMs_ = null;

    // If the pairing process required authentication, do not include the time
    // it took the user to complete or confirm the authentication process.
    return unadjustedPairingDurationMs - userAuthActionDurationMs;
  },
});
