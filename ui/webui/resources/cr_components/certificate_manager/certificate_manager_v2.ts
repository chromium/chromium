// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager-v2' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_manager_v2.html.js';
import type {SummaryCertInfo} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export class CertificateManagerV2Element extends PolymerElement {
  static get is() {
    return 'certificate-manager-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedTabIndex_: Number,
      tabNames_: Array,
      certificates: Array,
    };
  }

  override ready() {
    super.ready();
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.getChromeRootStoreCerts().then((results) => {
      this.certificates = results.crsCertInfos;
    });
  }

  private selectedTabIndex_ = 2;
  // TODO(crbug.com/40928765): Support localization.
  private tabNames_: string[] =
      ['Client Certificates', 'Local Certificates', 'Chrome Root Store'];
  certificates: SummaryCertInfo[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager-v2': CertificateManagerV2Element;
  }
}

customElements.define(
    CertificateManagerV2Element.is, CertificateManagerV2Element);
