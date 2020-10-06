// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview certificate-subentry represents an SSL certificate sub-entry.
 */

import '../../cr_elements/cr_action_menu/cr_action_menu.m.js';
import '../../cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../../cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import '../../cr_elements/policy/cr_policy_indicator.m.js';
import '../../cr_elements/icons.m.js';
import './certificate_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorType} from '../../cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nBehavior} from '../../js/i18n_behavior.m.js';

import {CertificateAction, CertificateActionEvent, CertificateActionEventDetail} from './certificate_manager_types.js';
import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificate-subentry',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificateSubnode} */
    model: Object,

    /** @type {!CertificateType} */
    certificateType: String,
  },

  /** @private {CertificatesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  },

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   * @param {!CertificateAction} action
   * @private
   */
  dispatchCertificateActionEvent_(action) {
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
  onRejected_(error) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // the native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.fire('certificates-error', {error: error, anchor: this.$.dots});
  },

  /** @private */
  onViewClick_() {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  },

  /** @private */
  onEditClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.EDIT);
  },

  /** @private */
  onDeleteClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.DELETE);
  },

  /** @private */
  onExportClick_() {
    this.closePopupMenu_();
    if (this.certificateType === CertificateType.PERSONAL) {
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
  canEdit_(model) {
    return model.canBeEdited;
  },

  /**
   * @param {!CertificateType} certificateType
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be exported.
   * @private
   */
  canExport_(certificateType, model) {
    if (certificateType === CertificateType.PERSONAL) {
      return model.extractable;
    }
    return true;
  },

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be deleted.
   * @private
   */
  canDelete_(model) {
    return model.canBeDeleted;
  },

  /** @private */
  closePopupMenu_() {
    this.$$('cr-action-menu').close();
  },

  /** @private */
  onDotsClick_() {
    const actionMenu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    actionMenu.showAt(this.$.dots);
  },

  /** @private */
  getPolicyIndicatorType_(model) {
    return model.policy ? CrPolicyIndicatorType.USER_POLICY :
                          CrPolicyIndicatorType.NONE;
  },
});
