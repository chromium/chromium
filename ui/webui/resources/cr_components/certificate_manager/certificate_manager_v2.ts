// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager-v2' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import './certificate_list_v2.js';
import './certificate_subpage_v2.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
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

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateListV2Element} from './certificate_list_v2.js';
import {getTemplate} from './certificate_manager_v2.html.js';
import type {CertManagementMetadata, ImportResult} from './certificate_manager_v2.mojom-webui.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import type {CertificateSubpageV2Element, SubpageCertificateList} from './certificate_subpage_v2.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export enum Page {
  LOCAL_CERTS = 'localcerts',
  CLIENT_CERTS = 'clientcerts',
  CRS_CERTS = 'crscerts',
  // Sub-pages
  ADMIN_CERTS = 'admincerts',
  PLATFORM_CERTS = 'platformcerts',
}

const CertificateManagerV2ElementBase = I18nMixin(PolymerElement);

export interface CertificateManagerV2Element {
  $: {
    crsCerts: CertificateListV2Element,
    toolbar: HTMLElement,
    platformClientCerts: CertificateListV2Element,
    // <if expr="is_win or is_macosx or is_linux">
    provisionedClientCerts: CertificateListV2Element,
    // </if>
    // <if expr="is_chromeos">
    extensionsClientCerts: CertificateListV2Element,
    // </if>
    toast: CrToastElement,
    dialog: CrDialogElement,
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
    adminCertsSection: CertificateSubpageV2Element,
    platformCertsSection: CertificateSubpageV2Element,
    numSystemCerts: HTMLElement,
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
      enterpriseSubpageLists_: {
        type: Array,
        value: () => {
          return [
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2TrustedCertsList'),
              certSource: CertificateSource.kEnterpriseTrustedCerts,
              hideExport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2IntermediateCertsList'),
              certSource: CertificateSource.kEnterpriseIntermediateCerts,
              hideExport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2DistrustedCertsList'),
              certSource: CertificateSource.kEnterpriseDistrustedCerts,
              hideExport: false,
            },
          ];
        },
      },
      platformSubpageLists_: {
        type: Array,
        value: () => {
          return [
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2TrustedCertsList'),
              certSource: CertificateSource.kPlatformUserTrustedCerts,
              hideExport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2IntermediateCertsList'),
              certSource: CertificateSource.kPlatformUserIntermediateCerts,
              hideExport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2DistrustedCertsList'),
              certSource: CertificateSource.kPlatformUserDistrustedCerts,
              hideExport: false,
            },
          ];
        },
      },

      toastMessage_: String,
      numSystemCertsString_: String,
      numPolicyCertsString_: String,
      crsLearnMoreUrl_: String,

      dialogTitle_: String,
      dialogBody_: String,

      showSearch_: {
        type: Boolean,
        value: false,
      },

      importOsCertsEnabled_: {
        type: Boolean,
        computed: 'computeImportOsCertsEnabled_(certManagementMetadata_)',
      },

      importOsCertsEnabledManaged_: {
        type: Boolean,
        computed: 'computeImportOsCertsManaged_(certManagementMetadata_)',
      },

      showClientCertImport_: Boolean,

      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },
    };
  }

  private selectedPage_: Page = Page.LOCAL_CERTS;
  private toastMessage_: string;
  private dialogTitle_: string;
  private dialogBody_: string;
  private numPolicyCertsString_: string;
  private numSystemCertsString_: string;
  private crsLearnMoreUrl_: string = loadTimeData.getString('crsLearnMoreUrl');
  private certManagementMetadata_: CertManagementMetadata;
  private importOsCertsEnabled_: boolean;
  private importOsCertsEnabledManaged_: boolean;
  private enterpriseSubpageLists_: SubpageCertificateList[];
  private platformSubpageLists_: SubpageCertificateList[];
  // <if expr="not chromeos_ash">
  private showClientCertImport_: boolean = false;
  // </if>
  // <if expr="chromeos_ash">
  // TODO(crbug.com/40928765): Import should also be disabled in kiosk mode or
  // when disabled by policy.
  private showClientCertImport_: boolean = true;
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

  private updateNumCertsStrings_() {
    if (this.certManagementMetadata_ === undefined) {
      this.numPolicyCertsString_ = '';
      this.numSystemCertsString_ = '';
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numPolicyCerts)
          .then(label => {
            this.numPolicyCertsString_ = label;
          });
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'certificateManagerV2NumCerts',
              this.certManagementMetadata_.numUserAddedSystemCerts)
          .then(label => {
            this.numSystemCertsString_ = label;
          });
    }
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

  private switchToPage_(page: Page) {
    this.selectedPage_ = page;
    switch (this.selectedPage_) {
      case Page.ADMIN_CERTS:
      case Page.PLATFORM_CERTS:
        this.$.toolbar.classList.add('toolbar-shadow');
        break;
      default:
        this.$.toolbar.classList.remove('toolbar-shadow');
    }
  }

  private onMenuItemSelect_(e: CustomEvent<{item: HTMLElement}>) {
    const page = e.detail.item.getAttribute('path');
    assert(page, 'Page is not available');
    this.switchToPage_(page as Page);
  }

  private getSelectedTopLevelPage_(): string {
    switch (this.selectedPage_) {
      case Page.ADMIN_CERTS:
      case Page.PLATFORM_CERTS:
        return Page.LOCAL_CERTS;
      default:
        return this.selectedPage_;
    }
  }

  private onPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    this.switchToPage_(Page.PLATFORM_CERTS);
    this.$.platformCertsSection.setInitialFocus();
  }

  private onAdminCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    this.switchToPage_(Page.ADMIN_CERTS);
    this.$.adminCertsSection.setInitialFocus();
  }

  // TODO(crbug.com/40928765): Make this work with multiple subpages, either by
  // adding a page name to the event payload, or making multiple different
  // navigateBack handlers (using the naming template
  // on<OptionalContext><EventName>_).
  private onNavigateBack_() {
    this.switchToPage_(Page.LOCAL_CERTS);
    focusWithoutInk(this.$.localMenuItem);
  }

  private onImportResult_(e: CustomEvent<ImportResult|null>) {
    const result = e.detail;
    if (result === null) {
      return;
    }
    if (result.error !== undefined) {
      // TODO(crbug.com/40928765): localize
      this.showDialog_('import result', result.error);
    }
  }

  private showDialog_(title: string, message: string) {
    this.dialogTitle_ = title;
    this.dialogBody_ = message;
    this.$.dialog.showModal();
  }

  private onDialogClickOk_() {
    this.$.dialog.close();
  }

  private computeImportOsCertsEnabled_(): boolean {
    return this.certManagementMetadata_.includeSystemTrustStore;
  }

  private computeImportOsCertsManaged_(): boolean {
    return this.certManagementMetadata_.isIncludeSystemTrustStoreManaged;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-manager-v2': CertificateManagerV2Element;
  }
}

customElements.define(
    CertificateManagerV2Element.is, CertificateManagerV2Element);
