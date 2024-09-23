// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An element that represents an SSL certificate entry.
 */
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_shared.css.js';
import './certificate_subentry.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_types.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_entry.html.js';
import type {CertificatesOrgGroup, CertificateType} from './certificates_browser_proxy.js';

const CertificateEntryElementBase = I18nMixin(PolymerElement);

class CertificateEntryElement extends CertificateEntryElementBase {
  static get is() {
    return 'certificate-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      certificateType: String,
    };
  }

  model: CertificatesOrgGroup;
  certificateType: CertificateType;

  /**
   * @return Whether the given index corresponds to the last sub-node.
   */
  private isLast_(index: number): boolean {
    return index === this.model.subnodes.length - 1;
  }

  private getPolicyIndicatorType_(): CrPolicyIndicatorType {
    return this.model.containsPolicyCerts ? CrPolicyIndicatorType.USER_POLICY :
                                            CrPolicyIndicatorType.NONE;
  }
}

customElements.define(CertificateEntryElement.is, CertificateEntryElement);
