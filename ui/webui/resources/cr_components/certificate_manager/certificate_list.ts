// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-list' is an element that displays a list of
 * certificates.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_entry.js';
import './certificate_shared.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_list.html.js';
import {CertificateAction, CertificateActionEvent} from './certificate_manager_types.js';
import type {CertificatesError, CertificatesImportError, CertificatesOrgGroup, NewCertificateSubNode} from './certificates_browser_proxy.js';
import {CertificatesBrowserProxyImpl, CertificateType} from './certificates_browser_proxy.js';

export interface CertificateListElement {
  $: {
    import: HTMLElement,
    // <if expr="is_chromeos">
    importAndBind: HTMLElement,
    // </if>
  };
}

const CertificateListElementBase = I18nMixin(PolymerElement);

export class CertificateListElement extends CertificateListElementBase {
  static get is() {
    return 'certificate-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certificates: {
        type: Array,
        value() {
          return [];
        },
      },

      certificateType: String,
      importAllowed: Boolean,

      // <if expr="is_chromeos">
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isGuest') &&
              loadTimeData.getBoolean('isGuest');
        },
      },
      // </if>

      isKiosk_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isKiosk') &&
              loadTimeData.getBoolean('isKiosk');
        },
      },
    };
  }

  certificates: CertificatesOrgGroup[];
  certificateType: CertificateType;
  importAllowed: boolean;
  // <if expr="is_chromeos">
  private isGuest_: boolean;
  // </if>
  private isKiosk_: boolean;

  private getDescription_(): string {
    if (this.certificates.length === 0) {
      return this.i18n('certificateManagerNoCertificates');
    }

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return this.i18n('certificateManagerYourCertificatesDescription');
      case CertificateType.SERVER:
        return this.i18n('certificateManagerServersDescription');
      case CertificateType.CA:
        return this.i18n('certificateManagerAuthoritiesDescription');
      case CertificateType.OTHER:
        return this.i18n('certificateManagerOthersDescription');
      default:
        assertNotReached();
    }
  }

  private canImport_(): boolean {
    return !this.isKiosk_ && this.certificateType !== CertificateType.OTHER &&
        this.importAllowed;
  }

  // <if expr="is_chromeos">
  private canImportAndBind_(): boolean {
    return !this.isGuest_ &&
        this.certificateType === CertificateType.PERSONAL && this.importAllowed;
  }
  // </if>

  /**
   * Handles a rejected Promise returned from |browserProxy_|.
   */
  private onRejected_(
      anchor: HTMLElement,
      error: CertificatesError|CertificatesImportError|null) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on a
      // native file chooser dialog or that the request was ignored by the
      // handler due to being received while another was still being processed.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.dispatchEvent(new CustomEvent('certificates-error', {
      bubbles: true,
      composed: true,
      detail: {error, anchor},
    }));
  }

  private dispatchImportActionEvent_(
      subnode: NewCertificateSubNode|null, anchor: HTMLElement) {
    this.dispatchEvent(new CustomEvent(CertificateActionEvent, {
      bubbles: true,
      composed: true,
      detail: {
        action: CertificateAction.IMPORT,
        subnode: subnode,
        certificateType: this.certificateType,
        anchor: anchor,
      },
    }));
  }

  private onImportClick_(e: Event) {
    this.handleImport_(false, e.target as HTMLElement);
  }

  // <if expr="is_chromeos">
  private onImportAndBindClick_(e: Event) {
    this.handleImport_(true, e.target as HTMLElement);
  }
  // </if>

  private handleImport_(useHardwareBacked: boolean, anchor: HTMLElement) {
    const browserProxy = CertificatesBrowserProxyImpl.getInstance();
    if (this.certificateType === CertificateType.PERSONAL) {
      browserProxy.importPersonalCertificate(useHardwareBacked)
          .then(showPasswordPrompt => {
            if (showPasswordPrompt) {
              this.dispatchImportActionEvent_(null, anchor);
            }
          }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType === CertificateType.CA) {
      browserProxy.importCaCertificate().then(certificateName => {
        this.dispatchImportActionEvent_({name: certificateName}, anchor);
      }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType === CertificateType.SERVER) {
      browserProxy.importServerCertificate().catch(
          this.onRejected_.bind(this, anchor));
    } else {
      assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-list': CertificateListElement;
  }
}

customElements.define(CertificateListElement.is, CertificateListElement);
