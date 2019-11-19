// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-list' is an element that displays a list of
 * certificates.
 */
Polymer({
  is: 'certificate-list',

  properties: {
    /** @type {!Array<!CertificatesOrgGroup>} */
    certificates: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @type {!CertificateType} */
    certificateType: String,

    /** @type {boolean} */
    importAllowed: Boolean,

    // 'if expr="chromeos"' here is breaking vulcanize. TODO(stevenjb/dpapad):
    // Restore after migrating to polymer-bundler, crbug.com/731881.
    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('isGuest') &&
            loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private */
    isKiosk_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('isKiosk') &&
            loadTimeData.getBoolean('isKiosk');
      },
    },
  },

  behaviors: [I18nBehavior],

  /**
   * @return {string}
   * @private
   */
  getDescription_: function() {
    if (this.certificates.length == 0) {
      return this.i18n('certificateManagerNoCertificates');
    }

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return this.i18n('certificateManagerYourCertificatesDescription');
      case CertificateType.SERVER:
        return this.i18n('certificateManagerServersDescription');
      case CertificateType.CA:
        return this.i18n('certificateManagerAuthoritiesDescription');
      case CertificateType.OTHER:
        return this.i18n('certificateManagerOthersDescription');
    }

    assertNotReached();
  },

  /**
   * @return {boolean}
   * @private
   */
  canImport_: function() {
    return !this.isKiosk_ && this.certificateType != CertificateType.OTHER &&
        this.importAllowed;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  canImportAndBind_: function() {
    return !this.isGuest_ && this.certificateType == CertificateType.PERSONAL &&
        this.importAllowed;
  },
  // </if>

  /**
   * Handles a rejected Promise returned from |browserProxy_|.
   * @param {!HTMLElement} anchor
   * @param {*} error Expects {!CertificatesError|!CertificatesImportError}.
   * @private
   */
  onRejected_: function(anchor, error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // a native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', {error: error, anchor: anchor});
  },


  /**
   * @param {?NewCertificateSubNode} subnode
   * @param {!HTMLElement} anchor
   * @private
   */
  dispatchImportActionEvent_: function(subnode, anchor) {
    this.fire(
        CertificateActionEvent,
        /** @type {!CertificateActionEventDetail} */ ({
          action: CertificateAction.IMPORT,
          subnode: subnode,
          certificateType: this.certificateType,
          anchor: anchor,
        }));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onImportTap_: function(e) {
    this.handleImport_(false, /** @type {!HTMLElement} */ (e.target));
  },

  // <if expr="chromeos">
  /**
   * @private
   * @param {!Event} e
   */
  onImportAndBindTap_: function(e) {
    this.handleImport_(true, /** @type {!HTMLElement} */ (e.target));
  },
  // </if>

  /**
   * @param {boolean} useHardwareBacked
   * @param {!HTMLElement} anchor
   * @private
   */
  handleImport_: function(useHardwareBacked, anchor) {
    const browserProxy =
        certificate_manager.CertificatesBrowserProxyImpl.getInstance();
    if (this.certificateType == CertificateType.PERSONAL) {
      browserProxy.importPersonalCertificate(useHardwareBacked)
          .then(showPasswordPrompt => {
            if (showPasswordPrompt) {
              this.dispatchImportActionEvent_(null, anchor);
            }
          }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType == CertificateType.CA) {
      browserProxy.importCaCertificate().then(certificateName => {
        this.dispatchImportActionEvent_({name: certificateName}, anchor);
      }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType == CertificateType.SERVER) {
      browserProxy.importServerCertificate().catch(
          this.onRejected_.bind(this, anchor));
    } else {
      assertNotReached();
    }
  },
});
