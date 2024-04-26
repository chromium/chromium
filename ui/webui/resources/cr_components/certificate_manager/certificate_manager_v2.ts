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
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

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

      // TODO(crbug.com/40928765): Split CRS tab out into its own HTML/TS file
      // pair.
      crsCertificates: Array,
      crsTrustedCertsOpened_: Boolean,
    };
  }

  override ready() {
    super.ready();
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.getChromeRootStoreCerts().then(
        (results: {crsCertInfos: SummaryCertInfo[]}) => {
          this.crsCertificates = results.crsCertInfos;
        });
  }

  private selectedTabIndex_: number = 0;
  // TODO(crbug.com/40928765): Support localization.
  private tabNames_: string[] =
      ['Client Certificates', 'Local Certificates', 'Chrome Root Store'];
  // TODO(crbug.com/40928765): This variable should be private, but is not right
  // now because the test at
  // chrome/test/data/webui/cr_components/certificate_manager_v2_test.ts
  // looks at this variable and not the DOM for testing. Test should be looking
  // at the DOM, and then this variable should be private.
  crsCertificates: SummaryCertInfo[] = [];
  private crsTrustedCertsOpened_: boolean = true;
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager-v2': CertificateManagerV2Element;
  }
}

customElements.define(
    CertificateManagerV2Element.is, CertificateManagerV2Element);
