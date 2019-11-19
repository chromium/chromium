// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager' component manages SSL certificates.
 */
Polymer({
  is: 'certificate-manager',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @type {number} */
    selected: {
      type: Number,
      value: 0,
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    personalCerts: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    serverCerts: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    caCerts: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    otherCerts: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Indicates if client certificate import is allowed
     * by Chrome OS specific policy ClientCertificateManagementAllowed.
     * Value exists only for Chrome OS.
     */
    clientImportAllowed: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates if CA certificate import is allowed
     * by Chrome OS specific policy CACertificateManagementAllowed.
     * Value exists only for Chrome OS.
     */
    caImportAllowed: {
      type: Boolean,
      value: false,
    },

    /** @private */
    certificateTypeEnum_: {
      type: Object,
      value: CertificateType,
      readOnly: true,
    },

    /** @private */
    showCaTrustEditDialog_: Boolean,

    /** @private */
    showDeleteConfirmationDialog_: Boolean,

    /** @private */
    showPasswordEncryptionDialog_: Boolean,

    /** @private */
    showPasswordDecryptionDialog_: Boolean,

    /** @private */
    showErrorDialog_: Boolean,

    /**
     * The model to be passed to dialogs that refer to a given certificate.
     * @private {?CertificateSubnode}
     */
    dialogModel_: Object,

    /**
     * The certificate type to be passed to dialogs that refer to a given
     * certificate.
     * @private {?CertificateType}
     */
    dialogModelCertificateType_: String,

    /**
     * The model to be passed to the error dialog.
     * @private {null|!CertificatesError|!CertificatesImportError}
     */
    errorDialogModel_: Object,

    /**
     * The element to return focus to, when the currently shown dialog is
     * closed.
     * @private {?HTMLElement}
     */
    activeDialogAnchor_: Object,

    /** @private */
    isKiosk_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('isKiosk') &&
            loadTimeData.getBoolean('isKiosk');
      },
    },

    /** @private {!Array<string>} */
    tabNames_: {
      type: Array,
      computed: 'computeTabNames_(isKiosk_)',
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('certificates-changed', this.set.bind(this));
    this.addWebUIListener(
        'client-import-allowed-changed',
        this.setClientImportAllowed.bind(this));
    this.addWebUIListener(
        'ca-import-allowed-changed', this.setCAImportAllowed.bind(this));
    certificate_manager.CertificatesBrowserProxyImpl.getInstance()
        .refreshCertificates();
  },

  /** @private */
  setClientImportAllowed: function(allowed) {
    this.clientImportAllowed = allowed;
  },

  /** @private */
  setCAImportAllowed: function(allowed) {
    this.caImportAllowed = allowed;
  },

  /**
   * @param {number} selectedIndex
   * @param {number} tabIndex
   * @return {boolean} Whether to show tab at |tabIndex|.
   * @private
   */
  isTabSelected_: function(selectedIndex, tabIndex) {
    return selectedIndex == tabIndex;
  },

  /** @override */
  ready: function() {
    this.addEventListener(CertificateActionEvent, event => {
      this.dialogModel_ = event.detail.subnode;
      this.dialogModelCertificateType_ = event.detail.certificateType;

      if (event.detail.action == CertificateAction.IMPORT) {
        if (event.detail.certificateType == CertificateType.PERSONAL) {
          this.openDialog_(
              'certificate-password-decryption-dialog',
              'showPasswordDecryptionDialog_', event.detail.anchor);
        } else if (event.detail.certificateType == CertificateType.CA) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        }
      } else {
        if (event.detail.action == CertificateAction.EDIT) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        } else if (event.detail.action == CertificateAction.DELETE) {
          this.openDialog_(
              'certificate-delete-confirmation-dialog',
              'showDeleteConfirmationDialog_', event.detail.anchor);
        } else if (event.detail.action == CertificateAction.EXPORT_PERSONAL) {
          this.openDialog_(
              'certificate-password-encryption-dialog',
              'showPasswordEncryptionDialog_', event.detail.anchor);
        }
      }

      event.stopPropagation();
    });

    this.addEventListener('certificates-error', event => {
      const detail =
          /** @type {!CertificatesErrorEventDetail} */ (event.detail);
      this.errorDialogModel_ = detail.error;
      this.openDialog_(
          'certificates-error-dialog', 'showErrorDialog_', detail.anchor);
      event.stopPropagation();
    });
  },

  /**
   * Opens a dialog and registers a listener for removing the dialog from the
   * DOM once is closed. The listener is destroyed when the dialog is removed
   * (because of 'restamp').
   *
   * @param {string} dialogTagName The tag name of the dialog to be shown.
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the dialog.
   * @param {?HTMLElement} anchor The element to focus when the dialog is
   *     closed. If null, the previous anchor element should be reused. This
   *     happens when a 'certificates-error-dialog' is opened, which when closed
   *     should focus the anchor of the previous dialog (the one that generated
   *     the error).
   * @private
   */
  openDialog_: function(dialogTagName, domIfBooleanName, anchor) {
    if (anchor) {
      this.activeDialogAnchor_ = anchor;
    }
    this.set(domIfBooleanName, true);
    this.async(() => {
      const dialog = this.$$(dialogTagName);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
        cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
      });
    });
  },

  /**
   * @return {!Array<string>}
   * @private
   */
  computeTabNames_: function() {
    return [
      loadTimeData.getString('certificateManagerYourCertificates'),
      ...(this.isKiosk_ ?
              [] :
              [
                loadTimeData.getString('certificateManagerServers'),
                loadTimeData.getString('certificateManagerAuthorities'),
              ]),
      loadTimeData.getString('certificateManagerOthers'),
    ];
  },
});
