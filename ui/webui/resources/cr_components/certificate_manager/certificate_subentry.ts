// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview certificate-subentry represents an SSL certificate sub-entry.
 */

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/cr_elements/icons_lit.html.js';
import './certificate_shared.css.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_types.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificateAction, CertificateActionEvent} from './certificate_manager_types.js';
import {getTemplate} from './certificate_subentry.html.js';
import type {CertificatesBrowserProxy, CertificatesError, CertificateSubnode} from './certificates_browser_proxy.js';
import {CertificatesBrowserProxyImpl, CertificateType} from './certificates_browser_proxy.js';

export interface CertificateSubentryElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
    dots: HTMLElement,
  };
}

const CertificateSubentryElementBase = I18nMixin(PolymerElement);

export class CertificateSubentryElement extends CertificateSubentryElementBase {
  static get is() {
    return 'certificate-subentry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      certificateType: String,
    };
  }

  model: CertificateSubnode;
  certificateType: CertificateType;
  private browserProxy_: CertificatesBrowserProxy =
      CertificatesBrowserProxyImpl.getInstance();

  /**
   * Dispatches an event indicating which certificate action was tapped. It is
   * used by the parent of this element to display a modal dialog accordingly.
   */
  private dispatchCertificateActionEvent_(action: CertificateAction) {
    this.dispatchEvent(new CustomEvent(CertificateActionEvent, {
      bubbles: true,
      composed: true,
      detail: {
        action: action,
        subnode: this.model,
        certificateType: this.certificateType,
        anchor: this.$.dots,
      },
    }));
  }

  /**
   * Handles the case where a call to the browser resulted in a rejected
   * promise.
   */
  private onRejected_(error: CertificatesError|null) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on a
      // native file chooser dialog or that the request was ignored by the
      // handler due to being received while another was still being processed.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.dispatchEvent(new CustomEvent('certificates-error', {
      bubbles: true,
      composed: true,
      detail: {error, anchor: null},
    }));
  }

  private onViewClick_() {
    this.closePopupMenu_();
    this.browserProxy_.viewCertificate(this.model.id);
  }

  private onEditClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.EDIT);
  }

  private onDeleteClick_() {
    this.closePopupMenu_();
    this.dispatchCertificateActionEvent_(CertificateAction.DELETE);
  }

  private onExportClick_() {
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
   * @return Whether the certificate can be edited.
   */
  private canEdit_(model: CertificateSubnode): boolean {
    return model.canBeEdited;
  }

  /**
   * @return Whether the certificate can be exported.
   */
  private canExport_(
      certificateType: CertificateType, model: CertificateSubnode): boolean {
    if (certificateType === CertificateType.PERSONAL) {
      return model.extractable;
    }
    return true;
  }

  /**
   * @return Whether the certificate can be deleted.
   */
  private canDelete_(model: CertificateSubnode): boolean {
    return model.canBeDeleted;
  }

  private closePopupMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private onDotsClick_() {
    this.$.menu.get().showAt(this.$.dots);
  }

  private getPolicyIndicatorType_(model: CertificateSubnode):
      CrPolicyIndicatorType {
    return model.policy ? CrPolicyIndicatorType.USER_POLICY :
                          CrPolicyIndicatorType.NONE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-subentry': CertificateSubentryElement;
  }
}

customElements.define(
    CertificateSubentryElement.is, CertificateSubentryElement);
