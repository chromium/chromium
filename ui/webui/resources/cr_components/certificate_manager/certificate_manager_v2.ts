// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager-v2' component is a newer way for
 * showing and managing TLS certificates. This is tied to the Chrome Root Store
 * and Chrome Cert Management Enterprise policies launch.
 */

import './certificate_list_v2.js';
import './certificate_info_dialog.js';
import './certificate_password_dialog.js';
import './certificate_subpage_v2.js';
import './certificate_manager_v2_icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/cr_elements/cr_page_host_style.css.js';

import {CrContainerShadowMixin} from '//resources/cr_elements/cr_container_shadow_mixin.js';
import type {CrPageSelectorElement} from '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateListV2Element} from './certificate_list_v2.js';
import {getTemplate} from './certificate_manager_v2.html.js';
import type {CertManagementMetadata, ImportResult} from './certificate_manager_v2.mojom-webui.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import type {CertificatePasswordDialogElement} from './certificate_password_dialog.js';
import type {CertificateSubpageV2Element, SubpageCertificateList} from './certificate_subpage_v2.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';
import type {Route} from './navigation_v2.js';
import {Page, RouteObserverMixin, Router} from './navigation_v2.js';

interface PasswordResult {
  password: string|null;
}

const CertificateManagerV2ElementBase =
    RouteObserverMixin(CrContainerShadowMixin(I18nMixin(PolymerElement)));

export interface CertificateManagerV2Element {
  $: {
    crsCerts: CertificateListV2Element,
    toolbar: HTMLElement,
    main: CrPageSelectorElement,
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
    viewOsImportedClientCerts: HTMLElement,
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
    adminCertsSection: CertificateSubpageV2Element,
    platformCertsSection: CertificateSubpageV2Element,
    platformClientCertsSection: CertificateSubpageV2Element,
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
              showImport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2IntermediateCertsList'),
              certSource: CertificateSource.kEnterpriseIntermediateCerts,
              hideExport: false,
              showImport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2DistrustedCertsList'),
              certSource: CertificateSource.kEnterpriseDistrustedCerts,
              hideExport: false,
              showImport: false,
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
              showImport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2IntermediateCertsList'),
              certSource: CertificateSource.kPlatformUserIntermediateCerts,
              hideExport: false,
              showImport: false,
            },
            {
              headerText: loadTimeData.getString(
                  'certificateManagerV2DistrustedCertsList'),
              certSource: CertificateSource.kPlatformUserDistrustedCerts,
              hideExport: false,
              showImport: false,
            },
          ];
        },
      },

      clientPlatformSubpageLists_: {
        type: Array,
        computed: 'computeClientPlatformSubpageLists_(showClientCertImport_)',
      },

      toastMessage_: String,
      numSystemCertsString_: String,
      numPolicyCertsString_: String,
      crsLearnMoreUrl_: String,

      showInfoDialog_: Boolean,
      infoDialogTitle_: String,
      infoDialogMessage_: String,
      showPasswordDialog_: Boolean,

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

      pageEnum_: {
        type: Object,
        value: Page,
      },
    };
  }

  private selectedPage_: Page;
  private toastMessage_: string;
  private showInfoDialog_: boolean = false;
  private infoDialogTitle_: string;
  private infoDialogMessage_: string;
  private showPasswordDialog_: boolean = false;
  private passwordEntryResolver_: PromiseResolver<PasswordResult>|null = null;
  private numPolicyCertsString_: string;
  private numSystemCertsString_: string;
  private crsLearnMoreUrl_: string = loadTimeData.getString('crsLearnMoreUrl');
  private certManagementMetadata_: CertManagementMetadata;
  private importOsCertsEnabled_: boolean;
  private importOsCertsEnabledManaged_: boolean;
  private enterpriseSubpageLists_: SubpageCertificateList[];
  private platformSubpageLists_: SubpageCertificateList[];
  private clientPlatformSubpageLists_: SubpageCertificateList[];
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
    proxy.callbackRouter.askForImportPassword.addListener(
        this.onAskForImportPassword_.bind(this));
    proxy.handler.getCertManagementMetadata().then(
        (results: {metadata: CertManagementMetadata}) => {
          this.certManagementMetadata_ = results.metadata;
          this.updateNumCertsStrings_();
        });
  }

  private onAskForImportPassword_(): Promise<PasswordResult> {
    this.showPasswordDialog_ = true;
    assert(this.passwordEntryResolver_ === null);
    this.passwordEntryResolver_ = new PromiseResolver<PasswordResult>();
    return this.passwordEntryResolver_.promise;
  }

  private onPasswordDialogClose_() {
    const passwordDialog =
        this.shadowRoot!.querySelector<CertificatePasswordDialogElement>(
            '#passwordDialog');
    assert(passwordDialog);
    assert(this.passwordEntryResolver_);
    this.passwordEntryResolver_.resolve({password: passwordDialog.value()});
    this.passwordEntryResolver_ = null;
    this.showPasswordDialog_ = false;
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

  override currentRouteChanged(route: Route, _: Route): void {
    this.selectedPage_ = route.page;

    switch (this.selectedPage_) {
      case Page.ADMIN_CERTS:
      case Page.PLATFORM_CERTS:
      case Page.PLATFORM_CLIENT_CERTS:
        // Sub-pages always show the top shadow, regardless of scroll position.
        this.enableScrollObservation(false);
        this.setForceDropShadows(true);
        break;
      default:
        // Main page uses scroll position to determine whether a shadow should
        // be shown.
        this.enableScrollObservation(true);
        this.setForceDropShadows(false);
    }
  }

  private onMenuItemActivate_(e: CustomEvent<{item: HTMLElement}>) {
    const page = e.detail.item.getAttribute('path');
    assert(page, 'Page is not available');
    Router.getInstance().navigateTo(page as Page);
  }

  private getSelectedTopLevelPage_(): string {
    switch (this.selectedPage_) {
      case Page.ADMIN_CERTS:
      case Page.PLATFORM_CERTS:
        return Page.LOCAL_CERTS;
      case Page.PLATFORM_CLIENT_CERTS:
        return Page.CLIENT_CERTS;
      default:
        return this.selectedPage_;
    }
  }

  private generateHrefForPage_(p: Page): string {
    return '/' + p;
  }

  private async onPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.PLATFORM_CERTS);
    await this.$.main.updateComplete;
    this.$.platformCertsSection.setInitialFocus();
  }

  private async onClientPlatformCertsLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.PLATFORM_CLIENT_CERTS);
    await this.$.main.updateComplete;
    this.$.platformClientCertsSection.setInitialFocus();
  }

  private async onAdminCertsInstalledLinkRowClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(Page.ADMIN_CERTS);
    await this.$.main.updateComplete;
    this.$.adminCertsSection.setInitialFocus();
  }

  private async onNavigateBack_(e: CustomEvent<{target: Page, source: Page}>) {
    Router.getInstance().navigateTo(e.detail.target);
    await this.$.main.updateComplete;
    switch (e.detail.source) {
      case Page.ADMIN_CERTS:
        const linkRow = this.shadowRoot!.querySelector<HTMLElement>(
            '#adminCertsInstalledLinkRow');
        assert(linkRow);
        focusWithoutInk(linkRow);
        break;
      case Page.PLATFORM_CERTS:
        focusWithoutInk(this.$.viewOsImportedCerts);
        break;
      case Page.PLATFORM_CLIENT_CERTS:
        focusWithoutInk(this.$.viewOsImportedClientCerts);
        break;
      default:
        // do nothing; shouldn't ever get here.
    }
  }

  private onImportResult_(e: CustomEvent<ImportResult|null>) {
    const result = e.detail;
    if (result === null) {
      return;
    }
    if (result.error !== undefined) {
      // TODO(crbug.com/40928765): localize
      this.infoDialogTitle_ = 'import result';
      this.infoDialogMessage_ = result.error;
      this.showInfoDialog_ = true;
    }
  }

  private onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  private computeImportOsCertsEnabled_(): boolean {
    return this.certManagementMetadata_.includeSystemTrustStore;
  }

  private computeImportOsCertsManaged_(): boolean {
    return this.certManagementMetadata_.isIncludeSystemTrustStoreManaged;
  }

  private computeClientPlatformSubpageLists_(): SubpageCertificateList[] {
    return [
      {
        headerText: loadTimeData.getString(
            'certificateManagerV2ClientCertsFromPlatform'),
        certSource: CertificateSource.kPlatformClientCert,
        hideExport: true,
        showImport: this.showClientCertImport_,
      },
    ];
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
