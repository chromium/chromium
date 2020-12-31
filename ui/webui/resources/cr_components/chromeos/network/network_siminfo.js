// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying cellular sim info.
 */

/** @enum {string} */
const ErrorType = {
  NONE: 'none',
  INCORRECT_PIN: 'incorrect-pin',
  INCORRECT_PUK: 'incorrect-puk',
  MISMATCHED_PIN: 'mismatched-pin',
  INVALID_PIN: 'invalid-pin',
  INVALID_PUK: 'invalid-puk'
};

(function() {

const DIGITS_ONLY_REGEX = /^[0-9]+$/;
const PIN_MIN_LENGTH = 4;
const PUK_MIN_LENGTH = 8;
const TOGGLE_DEBOUNCE_MS = 500;

Polymer({
  is: 'network-siminfo',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?OncMojo.DeviceStateProperties} */
    deviceState: {
      type: Object,
      value: null,
      observer: 'deviceStateChanged_',
    },

    /**
     * Reflects deviceState.simLockStatus.lockEnabled for the
     * toggle button.
     * @private
     */
    lockEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Set to true when a PUK is required to unlock the SIM.
     * @private
     */
    pukRequired_: {
      type: Boolean,
      value: false,
      observer: 'pukRequiredChanged_',
    },

    /**
     * Set to true when a SIM operation is in progress. Used to disable buttons.
     * @private
     */
    inProgress_: {
      type: Boolean,
      value: false,
      observer: 'updateSubmitButtonEnabled_',
    },

    /**
     * Set to an ErrorType value after an incorrect PIN or PUK entry.
     * @private {ErrorType}
     */
    error_: {
      type: Object,
      value: ErrorType.NONE,
      observer: 'updateSubmitButtonEnabled_',
    },

    /**
     * Properties enabling pin/puk enter/change buttons.
     * @private
     */
    enterPinEnabled_: Boolean,
    changePinEnabled_: Boolean,
    enterPukEnabled_: Boolean,

    /**
     * Properties reflecting pin/puk inputs.
     * @private
     */
    pin_: {
      type: String,
      observer: 'pinOrPukChange_',
    },
    pin_new1_: {
      type: String,
      observer: 'pinOrPukChange_',
    },
    pin_new2_: {
      type: String,
      observer: 'pinOrPukChange_',
    },
    puk_: {
      type: String,
      observer: 'pinOrPukChange_',
    },
  },

  /** @private {boolean} */
  sendSimLockEnabled_: false,

  /** @private {boolean|undefined} */
  setLockEnabled_: undefined,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  detached() {
    this.closeDialogs_();
  },

  /*
   * Returns the sim lock CrToggleElement.
   * @return {?CrToggleElement}
   */
  getSimLockToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#simLockButton'));
  },

  /** @private */
  closeDialogs_() {
    if (this.$.enterPinDialog.open) {
      this.onEnterPinDialogCancel_();
      this.$.enterPinDialog.close();
    }
    if (this.$.changePinDialog.open) {
      this.$.changePinDialog.close();
    }
    if (this.$.unlockPinDialog.open) {
      this.$.unlockPinDialog.close();
    }
    if (this.$.unlockPukDialog.open) {
      this.$.unlockPukDialog.close();
    }
  },

  /** @private */
  focusDialogInput_() {
    if (this.$.enterPinDialog.open) {
      this.$.enterPin.focus();
    } else if (this.$.changePinDialog.open) {
      this.$.changePinOld.focus();
    } else if (this.$.unlockPinDialog.open) {
      this.$.unlockPin.focus();
    } else if (this.$.unlockPukDialog.open) {
      this.$.unlockPuk.focus();
    }
  },

  /** @private */
  deviceStateChanged_() {
    if (!this.deviceState) {
      return;
    }
    const simLockStatus = this.deviceState.simLockStatus;
    if (!simLockStatus) {
      return;
    }
    this.pukRequired_ = simLockStatus.lockType === 'sim-puk';
    const lockEnabled = simLockStatus.lockEnabled;
    if (lockEnabled !== this.lockEnabled_) {
      this.setLockEnabled_ = lockEnabled;
      this.updateLockEnabled_();
    } else {
      this.setLockEnabled_ = undefined;
    }
  },

  /**
   * Wrapper method to prevent changing |lockEnabled_| while a dialog is open
   * to avoid confusion while a SIM operation is in progress. This must be
   * called after closing any dialog (and not opening another) to set the
   * correct state.
   * @private
   */
  updateLockEnabled_() {
    if (this.setLockEnabled_ === undefined || this.$.enterPinDialog.open ||
        this.$.changePinDialog.open || this.$.unlockPinDialog.open ||
        this.$.unlockPukDialog.open) {
      return;
    }
    this.lockEnabled_ = this.setLockEnabled_;
    this.setLockEnabled_ = undefined;
  },

  /** @private */
  delayUpdateLockEnabled_() {
    setTimeout(() => {
      this.updateLockEnabled_();
    }, TOGGLE_DEBOUNCE_MS);
  },

  /** @private */
  updateSubmitButtonEnabled_() {
    const hasError = this.error_ !== ErrorType.NONE;
    this.enterPinEnabled_ = !this.inProgress_ && !!this.pin_ && !hasError;
    this.changePinEnabled_ = !this.inProgress_ && !!this.pin_ &&
        !!this.pin_new1_ && !!this.pin_new2_ && !hasError;
    this.enterPukEnabled_ = !this.inProgress_ && !!this.puk_ &&
        !!this.pin_new1_ && !!this.pin_new2_ && !hasError;
  },

  /**
   * Clears error message on user interacion.
   * @private
   */
  pinOrPukChange_() {
    this.error_ = ErrorType.NONE;
    this.updateSubmitButtonEnabled_();
  },

  /** @private */
  pukRequiredChanged_() {
    if (this.$.unlockPukDialog.open) {
      if (this.pukRequired_) {
        this.$.unlockPuk.focus();
      } else {
        this.$.unlockPukDialog.close();
        this.delayUpdateLockEnabled_();
      }
      return;
    }

    if (!this.pukRequired_) {
      return;
    }

    // If the PUK was activated while attempting to enter or change a pin,
    // close the dialog and open the unlock PUK dialog.
    let showUnlockPuk = false;
    if (this.$.enterPinDialog.open) {
      this.$.enterPinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.changePinDialog.open) {
      this.$.changePinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.unlockPinDialog.open) {
      this.$.unlockPinDialog.close();
      showUnlockPuk = true;
    }
    if (!showUnlockPuk) {
      return;
    }

    this.showUnlockPukDialog_();
  },

  /**
   * Opens the pin dialog when the sim lock enabled state changes.
   * @param {!Event} event
   * @private
   */
  onSimLockEnabledChange_(event) {
    if (!this.deviceState) {
      return;
    }
    this.sendSimLockEnabled_ = event.target.checked;
    this.error_ = ErrorType.NONE;
    this.$.enterPin.value = '';
    this.$.enterPinDialog.showModal();
    requestAnimationFrame(() => {
      this.$.enterPin.focus();
    });
  },

  /** @private */
  setInProgress_() {
    this.error_ = ErrorType.NONE;
    this.inProgress_ = true;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.CellularSimState} cellularSimState
   * @private
   */
  setCellularSimState_(cellularSimState) {
    this.setInProgress_();
    this.networkConfig_.setCellularSimState(cellularSimState).then(response => {
      this.inProgress_ = false;
      if (!response.success) {
        this.error_ = ErrorType.INCORRECT_PIN;
        this.focusDialogInput_();
      } else {
        this.error_ = ErrorType.NONE;
        this.closeDialogs_();
        this.delayUpdateLockEnabled_();
      }
    });
    this.fire('user-action-setting-change');
  },

  /**
   * @param {string} pin
   * @param {string|undefined} puk
   * @private
   */
  unlockCellularSim_(pin, puk) {
    this.setInProgress_();
    const cellularSimState = {
      currentPinOrPuk: puk || pin,
      requirePin: false,
    };
    if (puk) {
      cellularSimState.newPin = pin;
    }
    this.networkConfig_.setCellularSimState(cellularSimState).then(response => {
      this.inProgress_ = false;
      if (!response.success) {
        this.error_ = puk ? ErrorType.INCORRECT_PUK : ErrorType.INCORRECT_PIN;
        this.focusDialogInput_();
      } else {
        this.error_ = ErrorType.NONE;
        this.closeDialogs_();
        this.delayUpdateLockEnabled_();
      }
    });
  },

  /**
   * Sends the PIN value from the Enter PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendEnterPin_(event) {
    event.stopPropagation();
    if (!this.enterPinEnabled_) {
      return;
    }
    const pin = this.$.enterPin.value;
    if (!this.validatePin_(pin)) {
      return;
    }
    const simState = {
      currentPinOrPuk: pin,
      requirePin: this.sendSimLockEnabled_,
    };
    this.setCellularSimState_(simState);
  },

  /**
   * Opens the Change PIN dialog.
   * @param {!Event} event
   * @private
   */
  onChangePinTap_(event) {
    event.stopPropagation();
    if (!this.deviceState) {
      return;
    }
    this.error_ = ErrorType.NONE;
    this.$.changePinOld.value = '';
    this.$.changePinNew1.value = '';
    this.$.changePinNew2.value = '';
    this.$.changePinDialog.showModal();
    requestAnimationFrame(() => {
      this.$.changePinOld.focus();
    });
  },

  /**
   * Sends the old and new PIN values from the Change PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendChangePin_(event) {
    event.stopPropagation();
    const newPin = this.$.changePinNew1.value;
    if (!this.validatePin_(newPin, this.$.changePinNew2.value)) {
      return;
    }
    const simState = {
      currentPinOrPuk: this.$.changePinOld.value,
      newPin: newPin,
      requirePin: true,
    };
    this.setCellularSimState_(simState);
  },

  /**
   * Opens the Unlock PIN / PUK dialog.
   * @param {!Event} event
   * @private
   */
  onUnlockPinTap_(event) {
    event.stopPropagation();
    if (this.pukRequired_) {
      this.showUnlockPukDialog_();
    } else {
      this.showUnlockPinDialog_();
    }
  },

  /**
   * Sends the PIN value from the Unlock PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendUnlockPin_(event) {
    event.stopPropagation();
    const pin = this.$.unlockPin.value;
    if (!this.validatePin_(pin)) {
      return;
    }
    this.unlockCellularSim_(pin, '');
  },

  /** @private */
  showUnlockPinDialog_() {
    this.error_ = ErrorType.NONE;
    this.$.unlockPin.value = '';
    this.$.unlockPinDialog.showModal();
    requestAnimationFrame(() => {
      this.$.unlockPin.focus();
    });
  },

  /** @private */
  showUnlockPukDialog_() {
    this.error_ = ErrorType.NONE;
    this.$.unlockPuk.value = '';
    this.$.unlockPin1.value = '';
    this.$.unlockPin2.value = '';
    this.$.unlockPukDialog.showModal();
    requestAnimationFrame(() => {
      this.$.unlockPuk.focus();
    });
  },

  /**
   * Sends the PUK value and new PIN value from the Unblock PUK dialog.
   * @param {!Event} event
   * @private
   */
  sendUnlockPuk_(event) {
    event.stopPropagation();
    const puk = this.$.unlockPuk.value;
    if (!this.validatePuk_(puk)) {
      return;
    }
    const pin = this.$.unlockPin1.value;
    if (!this.validatePin_(pin, this.$.unlockPin2.value)) {
      return;
    }
    this.unlockCellularSim_(pin, puk);
  },

  /**
   * @return {boolean}
   * @private
   */
  showSimMissing_() {
    return !!this.deviceState && !this.deviceState.simLockStatus;
  },

  /**
   * @return {boolean}
   * @private
   */
  showSimLocked_() {
    const simLockStatus = this.deviceState && this.deviceState.simLockStatus;
    if (!simLockStatus) {
      return false;
    }
    return !!simLockStatus.lockType;
  },

  /**
   * @return {boolean}
   * @private
   */
  showSimUnlocked_() {
    const simLockStatus = this.deviceState && this.deviceState.simLockStatus;
    if (!simLockStatus) {
      return false;
    }
    return !simLockStatus.lockType;
  },

  /** @private */
  getErrorMsg_() {
    if (this.error_ === ErrorType.NONE) {
      return '';
    } else if (this.error_ === ErrorType.MISMATCHED_PIN) {
      return this.i18n('networkSimErrorPinMismatch');
    }

    let errorStringId = '';
    switch (this.error_) {
      case ErrorType.INCORRECT_PIN:
        errorStringId = 'networkSimErrorIncorrectPin';
        break;
      case ErrorType.INCORRECT_PUK:
        errorStringId = 'networkSimErrorIncorrectPuk';
        break;
      case ErrorType.INVALID_PIN:
        errorStringId = 'networkSimErrorInvalidPin';
        break;
      case ErrorType.INVALID_PUK:
        errorStringId = 'networkSimErrorInvalidPuk';
        break;
      default:
        assertNotReached();
    }

    const retriesLeft = (this.deviceState && this.deviceState.simLockStatus) ?
        this.deviceState.simLockStatus.retriesLeft :
        0;
    if (retriesLeft !== 1) {
      errorStringId += 'Plural';
    }
    return this.i18n(errorStringId, retriesLeft);
  },

  /**
   * Checks whether |pin1| is of the proper length and contains only digits.
   * If opt_pin2 is not undefined, then it also checks whether pin1 and
   * opt_pin2 match. On any failure, sets |this.error_|, focuses the invalid
   * PIN, and returns false.
   * @param {string} pin1
   * @param {string=} opt_pin2
   * @return {boolean} True if the pins match and are of minimum length.
   * @private
   */
  validatePin_(pin1, opt_pin2) {
    if (!pin1.length) {
      return false;
    }
    if (pin1.length < PIN_MIN_LENGTH || !DIGITS_ONLY_REGEX.test(pin1)) {
      this.error_ = ErrorType.INVALID_PIN;
      this.focusDialogInput_();
      return false;
    }
    if (opt_pin2 !== undefined && pin1 !== opt_pin2) {
      this.error_ = ErrorType.MISMATCHED_PIN;
      this.focusDialogInput_();
      return false;
    }
    return true;
  },

  /**
   * Checks whether |puk| is of the proper length and contains only digits.
   * If not, sets |this.error_| and returns false.
   * @param {string} puk
   * @return {boolean} True if the puk is of minimum length.
   * @private
   */
  validatePuk_(puk) {
    if (puk.length < PUK_MIN_LENGTH || !DIGITS_ONLY_REGEX.test(puk)) {
      this.error_ = ErrorType.INVALID_PUK;
      return false;
    }
    return true;
  },

  /** @private */
  onEnterPinDialogCancel_() {
    this.lockEnabled_ = !!this.deviceState &&
        !!this.deviceState.simLockStatus &&
        this.deviceState.simLockStatus.lockEnabled;
  },

  /** @private */
  onEnterPinDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#simLockButton')));
  },

  /** @private */
  onChangePinDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#changePinButton')));
  },

  /** @private */
  onUnlockPinDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#unlockPinButton')));
  },
});
})();
