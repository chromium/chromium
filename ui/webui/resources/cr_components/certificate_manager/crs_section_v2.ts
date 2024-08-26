// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'crs-section-v2' component is the Chrome Root Store
 * section of the Certificate Management V2 UI.
 */

import './certificate_list_v2.js';
import './certificate_manager_style_v2.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_page_host_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateListV2Element} from './certificate_list_v2.js';
import {CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import {getTemplate} from './crs_section_v2.html.js';

const CrsSectionV2ElementBase = I18nMixin(PolymerElement);

export interface CrsSectionV2Element {
  $: {
    crsCerts: CertificateListV2Element,
  };
}

export class CrsSectionV2Element extends CrsSectionV2ElementBase {
  static get is() {
    return 'crs-section-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      crsLearnMoreUrl_: String,

      certificateSourceEnum_: {
        type: Object,
        value: CertificateSource,
      },
    };
  }

  private crsLearnMoreUrl_: string = loadTimeData.getString('crsLearnMoreUrl');
}

declare global {
  interface HTMLElementTagNameMap {
    'crs-section-v2': CrsSectionV2Element;
  }
}

customElements.define(CrsSectionV2Element.is, CrsSectionV2Element);
