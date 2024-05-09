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
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_entry_v2.html.js';
import {CertificatesV2BrowserProxy} from './certificates_v2_browser_proxy.js';

export interface CertificateEntryV2Element {
  $: {
    certhash: CrInputElement,
    copy: HTMLElement,
    view: HTMLElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'hash-copied': CustomEvent<void>;
  }
}


export class CertificateEntryV2Element extends PolymerElement {
  static get is() {
    return 'certificate-entry-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sha256hashHex: String,
      displayName: String,
    };
  }

  sha256hashHex: string;
  displayName: string;

  private onViewCertificate_() {
    CertificatesV2BrowserProxy.getInstance().handler.viewCertificate(
        this.sha256hashHex);
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
