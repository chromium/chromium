// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-list' is an element that displays a
 * list of certificate provisioning processes.
 */
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_provisioning_details_dialog.js';
import './certificate_provisioning_entry.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
import {CertificateProvisioningBrowserProxyImpl} from './certificate_provisioning_browser_proxy.js';
import {getTemplate} from './certificate_provisioning_list.html.js';

const CertificateProvisioningListElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class CertificateProvisioningListElement extends
    CertificateProvisioningListElementBase {
  static get is() {
    return 'certificate-provisioning-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      provisioningProcesses_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The model to be passed to certificate provisioning details dialog.
       */
      provisioningDetailsDialogModel_: Object,

      showProvisioningDetailsDialog_: Boolean,
    };
  }

  private provisioningProcesses_: CertificateProvisioningProcess[];
  private provisioningDetailsDialogModel_: CertificateProvisioningProcess|null;
  private showProvisioningDetailsDialog_: boolean;
  private previousAnchor_: HTMLElement|null = null;

  /**
   * @param provisioningProcesses The list of certificate provisioning
   *     processes.
   * @return Whether |provisioningProcesses| contains at least one entry.
   */
  private hasCertificateProvisioningEntries_(
      provisioningProcesses: CertificateProvisioningProcess[]): boolean {
    return provisioningProcesses.length !== 0;
  }

  /**
   * @param certProvisioningProcesses The currently active certificate
   *     provisioning processes
   */
  private onCertificateProvisioningProcessesChanged_(
      certProvisioningProcesses: CertificateProvisioningProcess[]) {
    this.provisioningProcesses_ = certProvisioningProcesses;

    // If a cert provisioning process details dialog is being shown, update its
    // model.
    if (!this.provisioningDetailsDialogModel_) {
      return;
    }

    const certProfileId = this.provisioningDetailsDialogModel_.certProfileId;
    const newDialogModel = this.provisioningProcesses_.find((process) => {
      return process.certProfileId === certProfileId;
    });
    if (newDialogModel) {
      this.provisioningDetailsDialogModel_ = newDialogModel;
    } else {
      // Close cert provisioning process details dialog if the process is no
      // longer in the list eg. when process completed successfully.
      this.shadowRoot!.querySelector(
                          'certificate-provisioning-details-dialog')!.close();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'certificate-provisioning-processes-changed',
        this.onCertificateProvisioningProcessesChanged_.bind(this));
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .refreshCertificateProvisioningProcesses();
  }

  override ready() {
    super.ready();
    this.addEventListener(
        CertificateProvisioningViewDetailsActionEvent, event => {
          const detail = event.detail;
          this.provisioningDetailsDialogModel_ = detail.model;
          this.previousAnchor_ = detail.anchor;
          this.showProvisioningDetailsDialog_ = true;
          event.stopPropagation();
          CertificateProvisioningBrowserProxyImpl.getInstance()
              .refreshCertificateProvisioningProcesses();
        });
  }

  private onDialogClose_() {
    this.showProvisioningDetailsDialog_ = false;
    focusWithoutInk(this.previousAnchor_!);
    this.previousAnchor_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-list': CertificateProvisioningListElement;
  }
}

customElements.define(
    CertificateProvisioningListElement.is, CertificateProvisioningListElement);
