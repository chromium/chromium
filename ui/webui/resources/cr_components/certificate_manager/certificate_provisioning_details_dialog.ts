// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-details-dialog' allows the user to
 * view the details of an in-progress certiifcate provisioning process.
 */
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
import {CertificateProvisioningBrowserProxyImpl} from './certificate_provisioning_browser_proxy.js';
import {getTemplate} from './certificate_provisioning_details_dialog.html.js';

export interface CertificateProvisioningDetailsDialogElement {
  $: {
    dialog: CrDialogElement,
    refresh: HTMLElement,
  };
}

const CertificateProvisioningDetailsDialogElementBase =
    I18nMixin(PolymerElement);

export class CertificateProvisioningDetailsDialogElement extends
    CertificateProvisioningDetailsDialogElementBase {
  static get is() {
    return 'certificate-provisioning-details-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      advancedExpanded_: Boolean,
    };
  }

  model: CertificateProvisioningProcess;
  private advancedExpanded_: boolean;

  close() {
    this.$.dialog.close();
  }

  private onRefresh_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessUpdate(this.model.certProfileId);
  }

  private onReset_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessReset(this.model.certProfileId);
  }

  private shouldHideLastFailedStatus_(): boolean {
    return this.model.lastUnsuccessfulMessage.length === 0;
  }

  private arrowState_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-details-dialog':
        CertificateProvisioningDetailsDialogElement;
  }
}

customElements.define(
    CertificateProvisioningDetailsDialogElement.is,
    CertificateProvisioningDetailsDialogElement);
