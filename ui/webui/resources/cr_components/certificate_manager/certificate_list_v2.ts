// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-list-v2' component shows a list of
 * certificates with a header, an expander, and optionally an "export all"
 * button.
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager_v2.ts.
 */


import './certificate_entry_v2.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_list_v2.html.js';
import type {CertificateSource, SummaryCertInfo} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

const CertificateListV2ElementBase = I18nMixin(PolymerElement);

export interface CertificateListV2Element {
  $: {
    certs: CrCollapseElement,
    exportCerts: HTMLElement,
    noCertsRow: HTMLElement,
  };
}

export class CertificateListV2Element extends CertificateListV2ElementBase {
  static get is() {
    return 'certificate-list-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certSource: Number,
      headerText: String,
      // True if the export button should be hidden.
      // Export button may also be hidden if there are no certs in the list.
      hideExport: Boolean,
      expanded_: Boolean,
      certificates_: Array,
      hasCerts_: {
        type: Boolean,
        computed: 'computeHasCerts_(certificates_)',
      },
    };
  }

  certSource: CertificateSource;
  headerText: string;
  hideExport: boolean = false;
  private expanded_: boolean = true;
  private certificates_: SummaryCertInfo[] = [];
  private hasCerts_: boolean;

  override ready() {
    super.ready();
    CertificatesV2BrowserProxy.getInstance()
        .handler.getCertificates(this.certSource)
        .then((results: {certs: SummaryCertInfo[]}) => {
          this.certificates_ = results.certs;
        });
  }

  private onExportCertsClick_(e: Event) {
    // Export button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesV2BrowserProxy.getInstance().handler.exportCertificates(
        this.certSource);
  }

  private computeHasCerts_(): boolean {
    return this.certificates_.length > 0;
  }

  private hideExportButton_(): boolean {
    return this.hideExport || !this.hasCerts_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-list-v2': CertificateListV2Element;
  }
}

customElements.define(CertificateListV2Element.is, CertificateListV2Element);
