// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure compiler typedefs.
 */

// clang-format off
// <if expr="is_chromeos">
import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
// </if>
import type {CertificatesError, CertificatesImportError,CertificateSubnode, CertificateType, NewCertificateSubNode} from './certificates_browser_proxy.js';
// clang-format on

/**
 * The payload of the 'certificate-action' event.
 */
export interface CertificateActionEventDetail {
  action: CertificateAction;
  subnode: CertificateSubnode|NewCertificateSubNode|null;
  certificateType: CertificateType;
  anchor: HTMLElement;
}

/**
 * The payload of the 'certificates-error' event.
 */
export interface CertificatesErrorEventDetail {
  error: CertificatesError|CertificatesImportError|null;
  anchor: HTMLElement|null;
}

/**
 * Enumeration of actions that require a popup menu to be shown to the user.
 */
export enum CertificateAction {
  DELETE = 0,
  EDIT = 1,
  EXPORT_PERSONAL = 2,
  IMPORT = 3,
}

/**
 * The name of the event fired when a certificate action is selected from the
 * dropdown menu. CertificateActionEventDetail is passed as the event detail.
 */
export const CertificateActionEvent = 'certificate-action';

// <if expr="is_chromeos">
/**
 * The payload of the 'certificate-provisioning-view-details-action' event.
 */
export interface CertificateProvisioningActionEventDetail {
  model: CertificateProvisioningProcess;
  anchor: HTMLElement;
}
// </if>

/**
 * The name of the event fired when a the "View Details" action is selected on
 * the dropdown menu next to a certificate provisioning process.
 * CertificateActionEventDetail is passed as the event detail.
 */
export const CertificateProvisioningViewDetailsActionEvent =
    'certificate-provisioning-view-details-action';

declare global {
  interface HTMLElementEventMap {
    'certificates-error': CustomEvent<CertificatesErrorEventDetail>;
    'certificate-action': CustomEvent<CertificateActionEventDetail>;
    // <if expr="is_chromeos">
    'certificate-provisioning-view-details-action':
        CustomEvent<CertificateProvisioningActionEventDetail>;
    // </if>
  }
}
