// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used on Chrome OS from the "Manage
 * certificates" section to interact with certificate provisioining processes.
 */

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * The 'certificate-provisioning-processes-changed' event will have an array of
 * CertificateProvisioningProcesses as its argument. This typedef is currently
 * declared here to be consistent with certificates_browser_proxy.js, but it is
 * not specific to CertificateProvisioningBrowserProxy.
 *
 * @typedef {{
 *   certProfileId: string,
 *   certProfileName: string,
 *   isDeviceWide: boolean,
 *   status: string,
 *   stateId: number,
 *   timeSinceLastUpdate: string,
 *   publicKey: string
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export let CertificateProvisioningProcess;

/** @interface */
export class CertificateProvisioningBrowserProxy {
  // <if expr="chromeos">
  // TODO(https://crbug.com/1071641): When it is possible to have conditional
  // imports in ui/webui/resources/cr_components/, this file should be
  // conditionally imported. Until then, it is imported unconditionally but its
  // non-exported code is omitted for non-ChromeOS platforms.

  /**
   * Refreshes the list of client certificate processes.
   * Triggers the 'certificate-provisioning-processes-changed' event.
   * This is Chrome OS specific, but always present for simplicity.
   */
  refreshCertificateProvisioningProcesses() {}

  /**
   * Attempts to manually advance/refresh the status of the client certificate
   * provisioning process identified by |certProfileId|.
   * This is Chrome OS specific, but always present for simplicity.
   * @param {string} certProfileId
   * @param {boolean} isDeviceWide
   */
  triggerCertificateProvisioningProcessUpdate(certProfileId, isDeviceWide) {}

  // </if>
}

/** @implements {CertificateProvisioningBrowserProxy} */
export class CertificateProvisioningBrowserProxyImpl {
  /** override */
  refreshCertificateProvisioningProcesses() {
    chrome.send('refreshCertificateProvisioningProcessses');
  }

  /** override */
  triggerCertificateProvisioningProcessUpdate(certProfileId, isDeviceWide) {
    chrome.send(
        'triggerCertificateProvisioningProcessUpdate',
        [certProfileId, isDeviceWide]);
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(CertificateProvisioningBrowserProxyImpl);
