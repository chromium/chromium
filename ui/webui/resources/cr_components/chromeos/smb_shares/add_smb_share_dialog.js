// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-smb-share-dialog' is a component for adding an SMB Share.
 *
 * This component can only be used once to add an SMB share, and must be
 * destroyed when finished, and re-created when shown again.
 */

cr.define('smb_shares', function() {
  /** @enum{number} */
  const MountErrorType = {
    NO_ERROR: 0,
    CREDENTIAL_ERROR: 1,
    PATH_ERROR: 2,
    GENERAL_ERROR: 3,
  };

  /**
   * Regular expression that matches SMB share URLs of the form
   * smb://server/share or \\server\share. This is a coarse regexp intended for
   * quick UI feedback and does not reject all invalid URLs.
   *
   * @type {!RegExp}
   */
  const SMB_SHARE_URL_REGEX =
      /^((smb:\/\/[^\/]+\/[^\/].*)|(\\\\[^\\]+\\[^\\].*))$/;

  return {
    MountErrorType: MountErrorType,
    SMB_SHARE_URL_REGEX: SMB_SHARE_URL_REGEX,
  };
});

Polymer({
  is: 'add-smb-share-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    lastUrl: {
      type: String,
      value: '',
    },

    shouldOpenFileManagerAfterMount: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    mountUrl_: {
      type: String,
      value: '',
      observer: 'onURLChanged_',
    },

    /** @private {string} */
    mountName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    username_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    password_: {
      type: String,
      value: '',
    },
    /** @private {!Array<string>}*/
    discoveredShares_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private */
    discoveryActive_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isActiveDirectory_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser');
      },
    },

    /** @private */
    authenticationMethod_: {
      type: String,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser') ?
            SmbAuthMethod.KERBEROS :
            SmbAuthMethod.CREDENTIALS;
      },
    },

    /** @private */
    generalErrorText_: String,

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {!smb_shares.MountErrorType} */
    currentMountError_: {
      type: Number,
      value: smb_shares.MountErrorType.NO_ERROR,
    },
  },

  /** @private {?smb_shares.SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = smb_shares.SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.startDiscovery();
    this.$.dialog.showModal();

    this.addWebUIListener('on-shares-found', this.onSharesFound_.bind(this));
    this.mountUrl_ = this.lastUrl;
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAddButtonTap_: function() {
    this.resetErrorState_();
    this.inProgress_ = true;
    this.browserProxy_
        .smbMount(
            this.mountUrl_, this.mountName_.trim(), this.username_,
            this.password_, this.authenticationMethod_,
            this.shouldOpenFileManagerAfterMount,
            this.$.saveCredentialsCheckbox.checked)
        .then(result => {
          this.onAddShare_(result);
        });
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onURLChanged_: function(newValue, oldValue) {
    this.resetErrorState_();
    const parts = this.mountUrl_.split('\\');
    this.mountName_ = parts[parts.length - 1];
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddShare_: function() {
    return !!this.mountUrl_ && !this.inProgress_ && this.isShareUrlValid_();
  },

  /**
   * @param {!Array<string>} newSharesDiscovered New shares that have been
   * discovered since the last call.
   * @param {boolean} done Whether share discovery has finished.
   * @private
   */
  onSharesFound_: function(newSharesDiscovered, done) {
    this.discoveredShares_ = this.discoveredShares_.concat(newSharesDiscovered);
    this.discoveryActive_ = !done;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialUI_: function() {
    return this.authenticationMethod_ == SmbAuthMethod.CREDENTIALS;
  },

  /**
   * @param {SmbMountResult} result
   * @private
   */
  onAddShare_: function(result) {
    this.inProgress_ = false;

    // Success case. Close dialog.
    if (result == SmbMountResult.SUCCESS) {
      this.$.dialog.close();
      return;
    }

    switch (result) {
      // Credential Error
      case SmbMountResult.AUTHENTICATION_FAILED:
        this.setCredentialError_(
            loadTimeData.getString('smbShareAddedAuthFailedMessage'));
        break;

      // Path Errors
      case SmbMountResult.NOT_FOUND:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedNotFoundMessage'));
        break;
      case SmbMountResult.INVALID_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidURLMessage'));
        break;
      case SmbMountResult.INVALID_SSO_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidSSOURLMessage'));
        break;

      // General Errors
      case SmbMountResult.UNSUPPORTED_DEVICE:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedUnsupportedDeviceMessage'));
        break;
      case SmbMountResult.MOUNT_EXISTS:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedMountExistsMessage'));
        break;
      default:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedErrorMessage'));
    }
  },

  /** @private */
  resetErrorState_: function() {
    this.currentMountError_ = smb_shares.MountErrorType.NO_ERROR;
    this.$.address.errorMessage = '';
    this.$.password.errorMessage = '';
    this.generalErrorText_ = '';
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setCredentialError_: function(errorMessage) {
    this.$.password.errorMessage = errorMessage;
    this.currentMountError_ = smb_shares.MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setGeneralError_: function(errorMessage) {
    this.generalErrorText_ = errorMessage;
    this.currentMountError_ = smb_shares.MountErrorType.GENERAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setPathError_: function(errorMessage) {
    this.$.address.errorMessage = errorMessage;
    this.currentMountError_ = smb_shares.MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialError_: function() {
    return this.currentMountError_ ==
        smb_shares.MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGeneralError_: function() {
    return this.currentMountError_ == smb_shares.MountErrorType.GENERAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPathError_: function() {
    return this.currentMountError_ == smb_shares.MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  isShareUrlValid_: function() {
    if (!this.mountUrl_ || this.shouldShowPathError_()) {
      return false;
    }
    return smb_shares.SMB_SHARE_URL_REGEX.test(this.mountUrl_);
  },
});
