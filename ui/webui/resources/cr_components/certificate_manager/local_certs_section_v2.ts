// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'local-certs-section-v2' component is a section of the
 * Certificate Management V2 UI that shows local modifications to the the users
 * trusted roots for TLS server auth (e.g. roots imported from the platform).
 */

import './certificate_manager_v2_icons.html.js';
import './certificate_manager_style_v2.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_page_host_style.css.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertManagementMetadata} from './certificate_manager_v2.mojom-webui.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';
import {getTemplate} from './local_certs_section_v2.html.js';
import {Page, Router} from './navigation_v2.js';


const LocalCertsSectionV2ElementBase = I18nMixin(PolymerElement);

export interface LocalCertsSectionV2Element {
  $: {
    importOsCerts: CrToggleElement,
    importOsCertsManagedIcon: HTMLElement,
    viewOsImportedCerts: HTMLElement,
    // <if expr="is_win or is_macosx">
    manageOsImportedCerts: HTMLElement,
    // </if>

    numSystemCerts: HTMLElement,
  };
}

export class LocalCertsSectionV2Element extends LocalCertsSectionV2ElementBase {
  static get is() {
    return 'local-certs-section-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // <if expr="not is_chromeos">
      numSystemCertsString_: String,
      // </if>
      numPolicyCertsString_: String,

      importOsCertsEnabled_: {
        type: Boolean,
        computed: 'computeImportOsCertsEnabled_(certManagementMetadata_)',
      },

      // <if expr="not is_chromeos">
      importOsCertsEnabledManaged_: {
        type: Boolean,
        computed: 'computeImportOsCertsManaged_(certManagementMetadata_)',
      },
      // </if>

      showViewOsCertsLinkRow_: {
        type: Boolean,
        computed: 'computeShowViewOsCertsLinkRow_(certManagementMetadata_)',
      },

      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },

      pageEnum_: {
        type: Object,
        value: Page,
      },
    };
  }

  private numPolicyCertsString_: string;
  // <if expr="not is_chromeos">
  private numSystemCertsString_: string;
  // </if>
  private certManagementMetadata_: CertManagementMetadata;
  private importOsCertsEnabled_: boolean;
  // <if expr="not is_chromeos">
  private importOsCertsEnabledManaged_: boolean;
  // </if>

  override ready() {
    super.ready();
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.getCertManagementMetadata().then(
        (results: {metadata: CertManagementMetadata}) => {
          this.certManagementMetadata_ = results.metadata;
          this.updateNumCertsStrings_();
        });
  }

  setFocusToLinkRow(p: Page) {
    switch (p) {
      case Page.ADMIN_CERTS:
        const linkRow = this.shadowRoot!.querySelector<HTMLElement>(
            '#adminCertsInstalledLinkRow');
        assert(linkRow);
        focusWithoutInk(linkRow);
        break;
      case Page.PLATFORM_CERTS:
        focusWithoutInk(this.$.viewOsImportedCerts);
        break;
      default:
        assertNotReached();
    }
  }

  private updateNumCertsStrings_() {
    if (this.certManagementMetadata_ === undefined) {
      this.numPolicyCertsString_ = '';
      // <if expr="not is_chromeos">
      this.numSystemCertsString_ = '';
      // </if>
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numPolicyCerts)
          .then(label => {
            this.numPolicyCertsString_ = label;
          });
      // <if expr="not is_chromeos">
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numUserAddedSystemCerts)
          .then(label => {
            this.numSystemCertsString_ = label;
          });
      // </if>
    }
  }

  private onPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.PLATFORM_CERTS);
  }

  private onAdminCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.ADMIN_CERTS);
  }

  private computeImportOsCertsEnabled_(): boolean {
    return this.certManagementMetadata_.includeSystemTrustStore;
  }

  // <if expr="not is_chromeos">
  private computeImportOsCertsManaged_(): boolean {
    return this.certManagementMetadata_.isIncludeSystemTrustStoreManaged;
  }
  // </if>

  private computeShowViewOsCertsLinkRow_(): boolean {
    return this.certManagementMetadata_ !== undefined &&
        this.certManagementMetadata_.numUserAddedSystemCerts > 0;
  }

  // If true, show the Custom Certs section.
  private showCustomSection_(): boolean {
    return this.certManagementMetadata_ !== undefined &&
        this.certManagementMetadata_.numPolicyCerts > 0;
  }

  // <if expr="is_win or is_macosx">
  private onManageCertsExternal_() {
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.showNativeManageCertificates();
  }
  // </if>

  // <if expr="not is_chromeos">
  private onOsCertsToggleChanged_(e: CustomEvent<boolean>) {
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.setIncludeSystemTrustStore(e.detail);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'local-certs-section-v2': LocalCertsSectionV2Element;
  }
}

customElements.define(
    LocalCertsSectionV2Element.is, LocalCertsSectionV2Element);
