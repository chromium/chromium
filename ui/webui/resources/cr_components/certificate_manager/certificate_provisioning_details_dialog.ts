// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-details-dialog' allows the user to
 * view the details of an in-progress certiifcate provisioning process.
 */
import '../../cr_elements/cr_expand_button/cr_expand_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';

import {CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
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
        .triggerCertificateProvisioningProcessUpdate(
            this.model.certProfileId, this.model.isDeviceWide);
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
