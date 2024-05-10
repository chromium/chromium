// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager-v2' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import './certificate_entry_v2.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_manager_v2.html.js';
import type {SummaryCertInfo} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export interface CertificateManagerV2Element {
  $: {
    crsCerts: CrCollapseElement,
    exportCRS: HTMLElement,
    toast: CrToastElement,
  };
}

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

      toastMessage_: String,

      // TODO(crbug.com/40928765): Split CRS tab out into its own HTML/TS file
      // pair. Or create some cert-list element and reuse that.
      crsCertificates_: Array,
      crsTrustedCertsOpened_: Boolean,
      platformClientCerts_: Array,
      // <if expr="is_win or is_macosx">
      provisionedClientCerts_: Array,
      // </if>
    };
  }

  private selectedTabIndex_: number = 0;
  // TODO(crbug.com/40928765): Support localization.
  private tabNames_: string[] =
      ['Client Certificates', 'Local Certificates', 'Chrome Root Store'];
  private toastMessage_: string;
  private crsCertificates_: SummaryCertInfo[] = [];
  private crsTrustedCertsOpened_: boolean = true;
  private platformClientCerts_: SummaryCertInfo[] = [];
  // <if expr="is_win or is_macosx">
  private provisionedClientCerts_: SummaryCertInfo[] = [];
  // </if>

  override ready() {
    super.ready();
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.getChromeRootStoreCerts().then(
        (results: {crsCertInfos: SummaryCertInfo[]}) => {
          this.crsCertificates_ = results.crsCertInfos;
        });

    proxy.handler.getPlatformClientCerts().then(
        (results: {certs: SummaryCertInfo[]}) => {
          this.platformClientCerts_ = results.certs;
        });

    // <if expr="is_win or is_macosx">
    proxy.handler.getProvisionedClientCerts().then(
        (results: {certs: SummaryCertInfo[]}) => {
          this.provisionedClientCerts_ = results.certs;
        });
    // </if>
  }

  private onValueCopied_() {
    // TODO(crbug.com/40928765): Support localization.
    this.toastMessage_ = 'Hash copied to clipboard';
    this.$.toast.show();
  }

  private onExportCrs_() {
    CertificatesV2BrowserProxy.getInstance().handler.exportChromeRootStore();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager-v2': CertificateManagerV2Element;
  }
}

customElements.define(
    CertificateManagerV2Element.is, CertificateManagerV2Element);
