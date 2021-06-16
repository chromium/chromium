// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-details-dialog' allows the user to
 * view the details of an in-progress certiifcate provisioning process.
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CertificateProvisioningDetailsDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CertificateProvisioningDetailsDialogElement extends
    CertificateProvisioningDetailsDialogElementBase {
  static get is() {
    return 'certificate-provisioning-details-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CertificateProvisioningProcess} */
      model: Object,

      /** @private */
      advancedExpanded_: Boolean,
    };
  }

  close() {
    /** @type {!CrDialogElement} */ (this.$.dialog).close();
  }

  /** @private */
  onRefresh_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessUpdate(
            this.model.certProfileId, this.model.isDeviceWide);
  }

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  }
}

customElements.define(
    CertificateProvisioningDetailsDialogElement.is,
    CertificateProvisioningDetailsDialogElement);
