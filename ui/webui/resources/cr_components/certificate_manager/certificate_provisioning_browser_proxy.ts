// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used on Chrome OS from the "Manage
 * certificates" section to interact with certificate provisioining processes.
 */

/**
 * The 'certificate-provisioning-processes-changed' event will have an array of
 * CertificateProvisioningProcesses as its argument. This typedef is currently
 * declared here to be consistent with certificates_browser_proxy.js, but it is
 * not specific to CertificateProvisioningBrowserProxy.
 *
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export interface CertificateProvisioningProcess {
  certProfileId: string;
  certProfileName: string;
  isDeviceWide: boolean;
  lastUnsuccessfulMessage: string;
  status: string;
  stateId: number;
  timeSinceLastUpdate: string;
  publicKey: string;
}

export interface CertificateProvisioningBrowserProxy {
  /**
   * Refreshes the list of client certificate processes.
   * Triggers the 'certificate-provisioning-processes-changed' event.
   */
  refreshCertificateProvisioningProcesses(): void;

  /**
   * Attempts to manually advance/refresh the status of the client certificate
   * provisioning process identified by |certProfileId|.
   */
  triggerCertificateProvisioningProcessUpdate(certProfileId: string): void;

  /**
   * Resets a particular certificate process.
   */
  triggerCertificateProvisioningProcessReset(certProfileId: string): void;
}

export class CertificateProvisioningBrowserProxyImpl implements
    CertificateProvisioningBrowserProxy {
  refreshCertificateProvisioningProcesses() {
    chrome.send('refreshCertificateProvisioningProcessses');
  }

  triggerCertificateProvisioningProcessUpdate(certProfileId: string) {
    chrome.send('triggerCertificateProvisioningProcessUpdate', [certProfileId]);
  }

  triggerCertificateProvisioningProcessReset(certProfileId: string) {
    chrome.send('triggerCertificateProvisioningProcessReset', [certProfileId]);
  }

  static getInstance(): CertificateProvisioningBrowserProxy {
    return instance ||
        (instance = new CertificateProvisioningBrowserProxyImpl());
  }

  static setInstance(obj: CertificateProvisioningBrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: CertificateProvisioningBrowserProxy|null = null;
