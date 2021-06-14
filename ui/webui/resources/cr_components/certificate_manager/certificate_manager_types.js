// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure compiler typedefs.
 */

// clang-format off
// <if expr="chromeos">
import {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
// </if>
import {CertificatesError, CertificatesImportError,CertificateSubnode, CertificateType, NewCertificateSubNode} from './certificates_browser_proxy.js';
// clang-format on

/**
 * The payload of the 'certificate-action' event.
 * @typedef {{
 *   action: !CertificateAction,
 *   subnode: (null|CertificateSubnode|NewCertificateSubNode),
 *   certificateType: !CertificateType,
 *   anchor: !HTMLElement
 * }}
 */
export let CertificateActionEventDetail;

/**
 * The payload of the 'certificates-error' event.
 * @typedef {{
 *   error: (null|CertificatesError|CertificatesImportError),
 *   anchor: ?HTMLElement
 * }}
 */
export let CertificatesErrorEventDetail;

/**
 * Enumeration of actions that require a popup menu to be shown to the user.
 * @enum {number}
 */
export const CertificateAction = {
  DELETE: 0,
  EDIT: 1,
  EXPORT_PERSONAL: 2,
  IMPORT: 3,
};

/**
 * The name of the event fired when a certificate action is selected from the
 * dropdown menu. CertificateActionEventDetail is passed as the event detail.
 */
export const CertificateActionEvent = 'certificate-action';

// <if expr="chromeos">
/**
 * The payload of the 'certificate-provisioning-view-details-action' event.
 * @typedef {{
 *   model: !CertificateProvisioningProcess,
 *   anchor: !HTMLElement
 * }}
 */
export let CertificateProvisioningActionEventDetail;
// </if>

/**
 * The name of the event fired when a the "View Details" action is selected on
 * the dropdown menu next to a certificate provisioning process.
 * CertificateActionEventDetail is passed as the event detail.
 */
export const CertificateProvisioningViewDetailsActionEvent =
    'certificate-provisioning-view-details-action';
