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

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorType} from '../../cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';

import {CertificateAction, CertificateActionEvent, CertificateActionEventDetail} from './certificate_manager_types.js';
import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, CertificateType} from './certificates_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CertificateSubentryElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CertificateSubentryElement extends CertificateSubentryElementBase {
  static get is() {
    return 'certificate-subentry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificateSubnode} */
      model: Object,

      /** @type {!CertificateType} */
      certificateType: String,
    };
  }

  constructor() {
    super();
    /** @private {!CertificatesBrowserProxy} */
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  }

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   * @param {!CertificateAction} action
   * @private
   */
  dispatchCertificateActionEvent_(action) {
    this.dispatchEvent(new CustomEvent(CertificateActionEvent, {
      bubbles: true,
      composed: true,
      detail: /** @type {!CertificateActionEventDetail} */ ({
        action: action,
        subnode: this.model,
        certificateType: this.certificateType,
        anchor: this.$.dots,
      }),
    }));
  }

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
    this.dispatchEvent(new CustomEvent('certificates-error', {
      bubbles: true,
      composed: true,
      detail: {error: error, anchor: null},
    }));
  }

  /** @private */
  onViewClick_() {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  }

  /** @private */
  onEditClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.EDIT);
  }

  /** @private */
  onDeleteClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.DELETE);
  }

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
  }

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be edited.
   * @private
   */
  canEdit_(model) {
    return model.canBeEdited;
  }

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
  }

  /**
   * @param {!CertificateSubnode} model
   * @return {boolean} Whether the certificate can be deleted.
   * @private
   */
  canDelete_(model) {
    return model.canBeDeleted;
  }

  /** @private */
  closePopupMenu_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
  }

  /** @private */
  onDotsClick_() {
    const actionMenu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    actionMenu.showAt(this.$.dots);
  }

  /** @private */
  getPolicyIndicatorType_(model) {
    return model.policy ? CrPolicyIndicatorType.USER_POLICY :
                          CrPolicyIndicatorType.NONE;
  }
}

customElements.define(
    CertificateSubentryElement.is, CertificateSubentryElement);
