// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager' component manages SSL certificates.
 */
import '../../cr_elements/cr_tabs/cr_tabs.js';
import '../../cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './ca_trust_edit_dialog.js';
import './certificate_delete_confirmation_dialog.js';
import './certificate_list.js';
import './certificate_password_decryption_dialog.js';
import './certificate_password_encryption_dialog.js';
import './certificates_error_dialog.js';
import './certificate_provisioning_list.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '../../js/assert.m.js';
import {focusWithoutInk} from '../../js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '../../js/i18n_behavior.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';
import {WebUIListenerBehavior} from '../../js/web_ui_listener_behavior.m.js';

import {CertificateAction, CertificateActionEvent, CertificatesErrorEventDetail} from './certificate_manager_types.js';
import {CertificatesBrowserProxyImpl, CertificatesError, CertificatesImportError, CertificatesOrgGroup, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificate-manager',

  _template: html`{__html_template__}`,

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
      value() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    serverCerts: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    caCerts: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @type {!Array<!CertificatesOrgGroup>} */
    otherCerts: {
      type: Array,
      value() {
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
      value() {
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
  attached() {
    this.addWebUIListener('certificates-changed', this.set.bind(this));
    this.addWebUIListener(
        'client-import-allowed-changed',
        this.setClientImportAllowed.bind(this));
    this.addWebUIListener(
        'ca-import-allowed-changed', this.setCAImportAllowed.bind(this));
    CertificatesBrowserProxyImpl.getInstance().refreshCertificates();
  },

  /** @private */
  setClientImportAllowed(allowed) {
    this.clientImportAllowed = allowed;
  },

  /** @private */
  setCAImportAllowed(allowed) {
    this.caImportAllowed = allowed;
  },

  /**
   * @param {number} selectedIndex
   * @param {number} tabIndex
   * @return {boolean} Whether to show tab at |tabIndex|.
   * @private
   */
  isTabSelected_(selectedIndex, tabIndex) {
    return selectedIndex === tabIndex;
  },

  /** @override */
  ready() {
    this.addEventListener(CertificateActionEvent, event => {
      this.dialogModel_ = event.detail.subnode;
      this.dialogModelCertificateType_ = event.detail.certificateType;

      if (event.detail.action === CertificateAction.IMPORT) {
        if (event.detail.certificateType === CertificateType.PERSONAL) {
          this.openDialog_(
              'certificate-password-decryption-dialog',
              'showPasswordDecryptionDialog_', event.detail.anchor);
        } else if (event.detail.certificateType === CertificateType.CA) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        }
      } else {
        if (event.detail.action === CertificateAction.EDIT) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        } else if (event.detail.action === CertificateAction.DELETE) {
          this.openDialog_(
              'certificate-delete-confirmation-dialog',
              'showDeleteConfirmationDialog_', event.detail.anchor);
        } else if (event.detail.action === CertificateAction.EXPORT_PERSONAL) {
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
  openDialog_(dialogTagName, domIfBooleanName, anchor) {
    if (anchor) {
      this.activeDialogAnchor_ = anchor;
    }
    this.set(domIfBooleanName, true);
    this.async(() => {
      const dialog = this.$$(dialogTagName);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
        focusWithoutInk(assert(this.activeDialogAnchor_));
      });
    });
  },

  /**
   * @return {!Array<string>}
   * @private
   */
  computeTabNames_() {
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
