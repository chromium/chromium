// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-subpage-v2' component is designed to show a
 * subpage. This subpage contains:
 *
 *   - header text
 *   - one or more lists of certs
 *   - a back button for navigating back to the previous page
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager_v2.ts.
 */

import './certificate_list_v2.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import {getTemplate} from './certificate_subpage_v2.html.js';
import {Page, Router} from './navigation_v2.js';

export interface CertificateSubpageV2Element {
  $: {
    backButton: HTMLElement,
  };
}

export class SubpageCertificateList {
  headerText: string;
  certSource: CertificateSource;
  hideExport?: boolean;
  showImport?: boolean;
  showImportAndBind?: boolean;
  hideIfEmpty?: boolean;
  hideHeader?: boolean;
}

const CertificateSubpageV2ElementBase = I18nMixin(PolymerElement);

export class CertificateSubpageV2Element extends
    CertificateSubpageV2ElementBase {
  static get is() {
    return 'certificate-subpage-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      subpageTitle: String,
      subpageCertLists: Array,
      navigateBackTarget: Page,
    };
  }

  subpageTitle: string;
  subpageCertLists: SubpageCertificateList[] = [];
  navigateBackTarget: Page;
  navigateBackSource: Page;

  // Sets initial keyboard focus of the subpage. Assumes that subpage elements
  // are visible.
  setInitialFocus() {
    focusWithoutInk(this.$.backButton);
  }

  private onBackButtonClick_(e: Event) {
    e.preventDefault();
    Router.getInstance().navigateTo(this.navigateBackTarget);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-subpage-v2': CertificateSubpageV2Element;
  }
}

customElements.define(
    CertificateSubpageV2Element.is, CertificateSubpageV2Element);
