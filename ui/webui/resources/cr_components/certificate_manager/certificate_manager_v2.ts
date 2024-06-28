// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager-v2' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import './certificate_enterprise_certs_v2.js';
import './certificate_list_v2.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/cr_elements/cr_page_host_style.css.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateEnterpriseCertsV2Element} from './certificate_enterprise_certs_v2.js';
import type {CertificateListV2Element} from './certificate_list_v2.js';
import {getTemplate} from './certificate_manager_v2.html.js';
import type {CertPolicyInfo} from './certificate_manager_v2.mojom-webui.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export enum Page {
  LOCAL_CERTS = 'localcerts',
  CLIENT_CERTS = 'clientcerts',
  CRS_CERTS = 'crscerts',
  // Sub-pages
  ADMIN_CERTS = 'admincerts',
}

const CertificateManagerV2ElementBase = I18nMixin(PolymerElement);

export interface CertificateManagerV2Element {
  $: {
    crsCerts: CertificateListV2Element,
    platformClientCerts: CertificateListV2Element,
    // <if expr="is_win or is_macosx or is_linux">
    provisionedClientCerts: CertificateListV2Element,
    // </if>
    // <if expr="is_chromeos">
    extensionsClientCerts: CertificateListV2Element,
    // </if>
    toast: CrToastElement,
    importOsCerts: CrToggleElement,
    importOsCertsManagedIcon: HTMLElement,
    viewOsImportedCerts: HTMLElement,
    // <if expr="is_win or is_macosx">
    manageOsImportedCerts: HTMLElement,
    manageOsImportedClientCerts: HTMLElement,
    // </if>

    localMenuItem: HTMLElement,
    clientMenuItem: HTMLElement,
    crsMenuItem: HTMLElement,

    localCertSection: HTMLElement,
    clientCertSection: HTMLElement,
    crsCertSection: HTMLElement,
    adminCertsInstalledLinkRow: HTMLElement,
    adminCertsSection: CertificateEnterpriseCertsV2Element,
  };
}

export class CertificateManagerV2Element extends
    CertificateManagerV2ElementBase {
  static get is() {
    return 'certificate-manager-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: String,
      tabNames_: Array,

      toastMessage_: String,

      showSearch_: {
        type: Boolean,
        value: false,
      },

      importOsCertsEnabled_: {
        type: Boolean,
        computed: 'computeImportOsCertsEnabled_(certPolicy_)',
      },

      importOsCertsEnabledManaged_: {
        type: Boolean,
        computed: 'computeImportOsCertsManaged_(certPolicy_)',
      },

      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },
    };
  }

  // TODO(crbug.com/40928765): create constants for paths for TS even if the
  // html has to have its own set of constants.
  private selectedPage_: Page = Page.LOCAL_CERTS;
  private toastMessage_: string;
  private certPolicy_: CertPolicyInfo;
  private importOsCertsEnabled_: boolean;
  private importOsCertsEnabledManaged_: boolean;

  override ready() {
    super.ready();
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.getPolicyInformation().then(
        (results: {policyInfo: CertPolicyInfo}) => {
          this.certPolicy_ = results.policyInfo;
        });
  }

  private onHashCopied_() {
    this.toastMessage_ =
        loadTimeData.getString('certificateManagerV2HashCopiedToast');
    this.$.toast.show();
  }

  // Prevent clicks on sidebar items from navigating and therefore reloading
  // the page.
  protected onMenuItemClick_(e: MouseEvent) {
    e.preventDefault();
  }

  private onMenuItemSelect_(e: CustomEvent<{item: HTMLElement}>) {
    const page = e.detail.item.getAttribute('path');
    assert(page, 'Page is not available');
    this.selectedPage_ = page as Page;
  }

  private getSelectedTopLevelPage_(): string {
    switch (this.selectedPage_) {
      case Page.ADMIN_CERTS:
        return Page.LOCAL_CERTS;
      default:
        return this.selectedPage_;
    }
  }

  private onAdminCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    this.selectedPage_ = Page.ADMIN_CERTS;
  }

  // TODO(crbug.com/40928765): Make this work with multiple subpages, either by
  // adding a page name to the event payload, or making multiple different
  // navigateBack handlers (using the naming template
  // on<OptionalContext><EventName>_).
  private onNavigateBack_() {
    this.selectedPage_ = Page.LOCAL_CERTS;
  }

  private computeImportOsCertsEnabled_(): boolean {
    return this.certPolicy_.includeSystemTrustStore;
  }

  private computeImportOsCertsManaged_(): boolean {
    return this.certPolicy_.isIncludeSystemTrustStoreManaged;
  }

  private getPolicyCertsString_(): string {
    if (this.certPolicy_ === undefined) {
      return '';
    }

    // TODO(crbug.com/40928765): Use PluralStringProxy instead.
    if (this.certPolicy_.numPolicyCerts > 1) {
      return loadTimeData.getStringF(
          'certificateManagerV2PolicyCertsPlural',
          this.certPolicy_.numPolicyCerts);
    } else {
      return loadTimeData.getString('certificateManagerV2PolicyCertsSingular');
    }
  }

  // If true, show the Custom Certs section.
  private showCustomSection_(): boolean {
    return this.certPolicy_ !== undefined &&
        this.certPolicy_.numPolicyCerts > 0;
  }

  // <if expr="is_win or is_macosx">
  private onManageCertsExternal_() {
    const proxy = CertificatesV2BrowserProxy.getInstance();
    proxy.handler.showNativeManageCertificates();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager-v2': CertificateManagerV2Element;
  }
}

customElements.define(
    CertificateManagerV2Element.is, CertificateManagerV2Element);
