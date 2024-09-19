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
import './certificate_manager_style_v2.css.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_list_v2.html.js';
import type {ActionResult, CertificateSource, SummaryCertInfo} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

const CertificateListV2ElementBase = I18nMixin(PolymerElement);

export interface CertificateListV2Element {
  $: {
    certs: CrCollapseElement,
    exportCerts: HTMLElement,
    importCert: HTMLElement,
    importAndBindCert: HTMLElement,
    noCertsRow: HTMLElement,
    listHeader: HTMLElement,
    expandButton: HTMLElement,
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
      showImport: Boolean,
      showImportAndBind: Boolean,

      // True if the list should not be collapsible.
      // Empty lists will always not be collapsible.
      noCollapse: Boolean,

      // True if the export button should be hidden.
      // Export button may also be hidden if there are no certs in the list.
      hideExport: Boolean,

      // True if the entire list (including the header) should be hidden if the
      // list is empty.
      hideIfEmpty: Boolean,

      // True if the header should be hidden. This will make the list
      // non-collapsible.
      hideHeader: Boolean,

      inSubpage: Boolean,
      expanded_: Boolean,
      certificates_: Array,
      hideEverything_: {
        type: Boolean,
        computed: 'computeHideEverything_(certificates_)',
      },
      hasCerts_: {
        type: Boolean,
        computed: 'computeHasCerts_(certificates_)',
      },
    };
  }

  certSource: CertificateSource;
  headerText: string;
  showImport: boolean = false;
  showImportAndBind: boolean = false;
  hideExport: boolean = false;
  hideHeader: boolean = false;
  inSubpage: boolean = false;
  noCollapse: boolean = false;
  hideIfEmpty: boolean = false;
  private expanded_: boolean = true;
  private certificates_: SummaryCertInfo[] = [];
  private hasCerts_: boolean;

  override ready() {
    super.ready();

    this.refreshCertificates();

    if (!this.inSubpage) {
      this.$.certs.classList.add('card');
    }
    if (this.inSubpage) {
      this.$.listHeader.classList.add('subpage-padding');
    }
  }

  private refreshCertificates() {
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

  private onImportCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesV2BrowserProxy.getInstance()
        .handler.importCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  private onImportAndBindCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesV2BrowserProxy.getInstance()
        .handler.importAndBindCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  private handleImportResult(value: {result: ActionResult|null}) {
    if (value.result !== null && value.result.success !== undefined) {
      // On successful import, refresh the certificate list.
      this.refreshCertificates();
    }
    this.dispatchEvent(new CustomEvent(
        'import-result',
        {composed: true, bubbles: true, detail: value.result}));
  }

  private onDeleteResult_(e: CustomEvent<ActionResult|null>) {
    const result = e.detail;
    if (result !== null && result.success !== undefined) {
      // On successful deletion, refresh the certificate list.
      this.refreshCertificates();
    }
  }

  private computeHasCerts_(): boolean {
    return this.certificates_.length > 0;
  }

  private computeHideEverything_(): boolean {
    return this.hideIfEmpty && this.certificates_.length === 0;
  }

  private hideCollapseButton_(): boolean {
    return this.noCollapse || !this.hasCerts_;
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
