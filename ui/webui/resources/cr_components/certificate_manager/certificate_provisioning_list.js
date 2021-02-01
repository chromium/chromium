// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-list' is an element that displays a
 * list of certificate provisioning processes.
 */
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_provisioning_details_dialog.js';
import './certificate_provisioning_entry.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificateProvisioningActionEventDetail, CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import {CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

// <if expr="chromeos">
// TODO(https://crbug.com/1071641): When it is possible to have conditional
// imports in ui/webui/resources/cr_components/, this file should be
// conditionally imported. Until then, it is imported unconditionally but its
// contents are omitted for non-ChromeOS platforms.

Polymer({
  is: 'certificate-provisioning-list',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!CertificateProvisioningProcess>} */
    provisioningProcesses_: {
      type: Array,
      value() {
        return [];
      }
    },

    /**
     * The model to be passed to certificate provisioning details dialog.
     * @private {?CertificateProvisioningProcess}
     */
    provisioningDetailsDialogModel_: Object,

    /** @private */
    showProvisioningDetailsDialog_: Boolean,
  },

  /**
   * @param {!Array<!CertificateProvisioningProcess>} provisioningProcesses The
   * list of certificate provisioning processes.
   * @return {boolean} true if |provisioningProcesses| contains at least one
   * entry.
   * @private
   */
  hasCertificateProvisioningEntries_(provisioningProcesses) {
    return provisioningProcesses.length !== 0;
  },

  /**
   * @param {!Array<!CertificateProvisioningProcess>} certProvisioningProcesses
   *    The currently active certificate provisioning processes
   * @private
   */
  onCertificateProvisioningProcessesChanged_(certProvisioningProcesses) {
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
      this.$$('certificate-provisioning-details-dialog').close();
    }
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'certificate-provisioning-processes-changed',
        this.onCertificateProvisioningProcessesChanged_.bind(this));
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .refreshCertificateProvisioningProcesses();
  },

  /** @override */
  ready() {
    this.addEventListener(
        CertificateProvisioningViewDetailsActionEvent, event => {
          const detail =
              /** @type {!CertificateProvisioningActionEventDetail} */ (
                  event.detail);
          this.provisioningDetailsDialogModel_ = detail.model;

          const previousAnchor = assert(detail.anchor);
          this.showProvisioningDetailsDialog_ = true;
          this.async(() => {
            const dialog = this.$$('certificate-provisioning-details-dialog');
            // The listener is destroyed when the dialog is removed (because of
            // 'restamp').
            dialog.addEventListener('close', () => {
              this.showProvisioningDetailsDialog_ = false;
              focusWithoutInk(previousAnchor);
            });
          });

          event.stopPropagation();
        });
  }
});

// </if>
