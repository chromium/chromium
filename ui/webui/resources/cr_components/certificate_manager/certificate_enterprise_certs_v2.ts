// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-enterprise-certs-v2' component shows a summary
 * of all enterprise certificates used for server certificate verification.
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager_v2.ts.
 */

import './certificate_list_v2.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_enterprise_certs_v2.html.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';

const CertificateEnterpriseCertsV2ElementBase = I18nMixin(PolymerElement);

export interface CertificateEnterpriseCertsV2Element {
  $: {
    backButton: HTMLElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'navigate-back': CustomEvent<void>;
  }
}

export class CertificateEnterpriseCertsV2Element extends
    CertificateEnterpriseCertsV2ElementBase {
  static get is() {
    return 'certificate-enterprise-certs-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },
    };
  }

  private onBackButtonClick_(e: Event) {
    e.preventDefault();
    this.dispatchEvent(
        new CustomEvent('navigate-back', {composed: true, bubbles: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-enterprise-certs-v2': CertificateEnterpriseCertsV2Element;
  }
}

customElements.define(
    CertificateEnterpriseCertsV2Element.is,
    CertificateEnterpriseCertsV2Element);
