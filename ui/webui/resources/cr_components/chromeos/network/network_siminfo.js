// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying cellular sim info.
 */

(function() {

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

    /** @type {?OncMojo.NetworkStateProperties} */
    networkState: {
      type: Object,
      value: null,
    },

    disabled: {
      type: Boolean,
      value: false,
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

    /** @private {boolean} */
    isDialogOpen_: {
      type: Boolean,
      value: false,
      observer: 'onDialogOpenChanged_',
    },

    /**
     * If set to true, shows the Change PIN dialog if the device is unlocked.
     * @private {boolean}
     */
    showChangePin_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates that the current network is on the active sim slot.
     * @private {boolean}
     */
    isActiveSim_: {
      type: Boolean,
      value: false,
      computed: 'computeIsActiveSim_(networkState, deviceState)'
    },

    /** @private */
    isUpdatedCellularUiEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('updatedCellularActivationUi');
      }
    },
  },

  /** @private {boolean|undefined} */
  setLockEnabled_: undefined,

  /*
   * Returns the sim lock CrToggleElement.
   * @return {?CrToggleElement}
   */
  getSimLockToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#simLockButton'));
  },

  /** @private */
  onDialogOpenChanged_() {
    if (this.isDialogOpen_) {
      return;
    }

    this.delayUpdateLockEnabled_();
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
    if (this.setLockEnabled_ === undefined || this.isDialogOpen_) {
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

  /**
   * Opens the pin dialog when the sim lock enabled state changes.
   * @param {!Event} event
   * @private
   */
  onSimLockEnabledChange_(event) {
    if (!this.deviceState) {
      return;
    }
    // Do not change the toggle state after toggle is clicked. The toggle
    // should only be updated when the device state changes or dialog has been
    // closed. Changing the UI toggle before the device state changes or dialog
    // is closed can be confusing to the user, as it indicates the action was
    // successful.
    this.lockEnabled_ = !this.lockEnabled_;
    this.showSimLockDialog_(/*showChangePin=*/ false);
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
    this.showSimLockDialog_(true);
  },

  /**
   * Opens the Unlock PIN / PUK dialog.
   * @param {!Event} event
   * @private
   */
  onUnlockPinTap_(event) {
    event.stopPropagation();
    this.showSimLockDialog_(true);
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
    return !!simLockStatus.lockType && this.isActiveSim_;
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

  /**
   * @param {boolean} showChangePin
   * @private
   */
  showSimLockDialog_(showChangePin) {
    this.showChangePin_ = showChangePin;
    this.isDialogOpen_ = true;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsActiveSim_() {
    const mojom = chromeos.networkConfig.mojom;
    if (!this.networkState ||
        this.networkState.type !== mojom.NetworkType.kCellular) {
      return false;
    }

    const iccid = this.networkState.typeState.cellular.iccid;
    if (!iccid || !this.deviceState || !this.deviceState.simInfos) {
      return false;
    }
    const isActiveSim = this.deviceState.simInfos.find(simInfo => {
      return simInfo.iccid === iccid && simInfo.isPrimary;
    });

    return !!isActiveSim;
  },

  /**
   * @return {boolean}
   * @private
   */
  showChangePinButton_() {
    if (!this.deviceState || !this.deviceState.simLockStatus) {
      return false;
    }

    return this.deviceState.simLockStatus.lockEnabled && this.isActiveSim_;
  },

  /**
   * @return {boolean}
   * @private
   */
  isSimLockButtonDisabled_() {
    return this.disabled || !this.isActiveSim_;
  },
});
})();
