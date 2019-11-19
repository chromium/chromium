// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview certificate-subentry represents an SSL certificate sub-entry.
 */

Polymer({
  is: 'certificate-subentry',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificateSubnode} */
    model: Object,

    /** @type {!CertificateType} */
    certificateType: String,
  },

  /** @private {certificate_manager.CertificatesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ =
        certificate_manager.CertificatesBrowserProxyImpl.getInstance();
  },

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   * @param {!CertificateAction} action
   * @private
   */
  dispatchCertificateActionEvent_: function(action) {
    this.fire(
        CertificateActionEvent,
        /** @type {!CertificateActionEventDetail} */ ({
          action: action,
          subnode: this.model,
          certificateType: this.certificateType,
          anchor: this.$.dots,
        }));
  },

  /**
   * Handles the case where a call to the browser resulted in a rejected
   * promise.
   * @param {*} error Expects {?CertificatesError}.
   * @private
   */
  onRejected_: function(error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // the native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', {error: error, anchor: this.$.dots});
  },

  /**
   * @param {!Event} event
   * @private
   */
  onViewTap_: function(event) {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEditTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.EDIT);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDeleteTap_: function(event) {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.DELETE);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onExportTap_: function(event) {
    this.closePopupMenu_();
    if (this.certificateType == CertificateType.PERSONAL) {
      this.browserProxy_.exportPersonalCertificate(this.model.id).then(() => {
        this.dispatchCertificateActionEvent_(CertificateAction.EXPORT_PERSONAL);
      }, this.onRejected_.bind(this));
    } else {
      this.browserProxy_.exportCertificate(this.model.id);
    }
  },

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be edited.
   * @private
   */
  canEdit_: function(model) {
    return model.canBeEdited;
  },

  /**
   * @param {!CertificateType} certificateType
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be exported.
   * @private
   */
  canExport_: function(certificateType, model) {
    if (certificateType == CertificateType.PERSONAL) {
      return model.extractable;
    }
    return true;
  },

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be deleted.
   * @private
   */
  canDelete_: function(model) {
    return model.canBeDeleted;
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('cr-action-menu').close();
  },

  /** @private */
  onDotsTap_: function() {
    const actionMenu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    actionMenu.showAt(this.$.dots);
  },

  /** @private */
  getPolicyIndicatorType_: function(model) {
    return model.policy ? CrPolicyIndicatorType.USER_POLICY :
                          CrPolicyIndicatorType.NONE;
  },
});
