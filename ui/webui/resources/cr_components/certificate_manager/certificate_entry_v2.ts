// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-entry-v2' component is for showing a summary
 * of a certificate in a row on screen.
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager_v2.ts.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_entry_v2.html.js';
import type {ActionResult, CertificateSource} from './certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export interface CertificateEntryV2Element {
  $: {
    certhash: CrInputElement,
    copy: HTMLElement,
    view: HTMLElement,
    delete: HTMLElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'hash-copied': CustomEvent<void>;
    'delete-result': CustomEvent<ActionResult|null>;
  }
}

const CertificateEntryV2ElementBase = I18nMixin(PolymerElement);

export class CertificateEntryV2Element extends CertificateEntryV2ElementBase {
  static get is() {
    return 'certificate-entry-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certSource: Number,
      sha256hashHex: String,
      displayName: String,
      isDeletable: Boolean,
    };
  }

  certSource: CertificateSource;
  sha256hashHex: string;
  displayName: string;
  isDeletable: boolean;

  private onViewCertificate_() {
    CertificatesV2BrowserProxy.getInstance().handler.viewCertificate(
        this.certSource, this.sha256hashHex);
  }

  private onDeleteCertificate_() {
    assert(this.isDeletable);
    CertificatesV2BrowserProxy.getInstance()
        .handler.deleteCertificate(this.certSource, this.sha256hashHex)
        .then((value: {result: ActionResult|null}) => {
          this.dispatchEvent(new CustomEvent('delete-result', {
            composed: true,
            bubbles: true,
            detail: value.result,
          }));
        });
  }

  private onCopyHash_() {
    navigator.clipboard.writeText(this.sha256hashHex);
    this.dispatchEvent(
        new CustomEvent('hash-copied', {composed: true, bubbles: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-entry-v2': CertificateEntryV2Element;
  }
}

customElements.define(CertificateEntryV2Element.is, CertificateEntryV2Element);
