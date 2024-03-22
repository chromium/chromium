// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage certificates" section
 * to interact with the browser.
 */

import {sendWithPromise} from '//resources/js/cr.js';

/**
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export interface CertificateSubnode {
  extractable: boolean;
  id: string;
  name: string;
  policy: boolean;
  webTrustAnchor: boolean;
  canBeDeleted: boolean;
  canBeEdited: boolean;
  untrusted: boolean;
}

/**
 * A data structure describing a certificate that is currently being imported,
 * therefore it has no ID yet, but it has a name. Used within JS only.
 */
export interface NewCertificateSubNode {
  name: string;
}

/**
 * Top-level grouping node in a certificate list, representing an organization
 * and containing certs that belong to the organization in |subnodes|. If a
 * certificate does not have an organization name, it will be grouped under its
 * own CertificatesOrgGroup with |name| set to its display name.
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export interface CertificatesOrgGroup {
  id: string;
  name: string;
  containsPolicyCerts: boolean;
  subnodes: CertificateSubnode[];
}

export interface CaTrustInfo {
  ssl: boolean;
  email: boolean;
  objSign: boolean;
}

/**
 * Generic error returned from C++ via a Promise reject callback.
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export interface CertificatesError {
  title: string;
  description: string;
}

/**
 * Enumeration of all possible certificate types.
 */
export enum CertificateType {
  CA = 'ca',
  OTHER = 'other',
  PERSONAL = 'personal',
  SERVER = 'server',
}


/**
 * Error returned from C++ via a Promise reject callback, when some certificates
 * fail to be imported.
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export interface CertificatesImportError {
  title: string;
  description: string;
  certificateErrors: Array<{name: string, error: string}>;
}

export interface CertificatesBrowserProxy {
  /**
   * Triggers 5 events in the following order
   * 1x 'client-import-allowed-changed' event.
   * 1x 'ca-import-allowed-changed' event.
   * 4x 'certificates-changed' event, one for each certificate category.
   */
  refreshCertificates(): void;

  viewCertificate(id: string): void;

  exportCertificate(id: string): void;

  /**
   * @return A promise resolved when the certificate has been
   *     deleted successfully or rejected with a CertificatesError.
   */
  deleteCertificate(id: string): Promise<void>;

  getCaCertificateTrust(id: string): Promise<CaTrustInfo>;

  editCaCertificateTrust(
      id: string, ssl: boolean, email: boolean,
      objSign: boolean): Promise<void>;

  cancelImportExportCertificate(): void;

  /**
   * @return A promise firing once the user has selected
   *     the export location. A prompt should be shown to asking for a
   *     password to use for encrypting the file. The password should be
   *     passed back via a call to
   *     exportPersonalCertificatePasswordSelected().
   */
  exportPersonalCertificate(id: string): Promise<void>;

  exportPersonalCertificatePasswordSelected(password: string): Promise<void>;

  /**
   * @return A promise firing once the user has selected
   *     the file to be imported. If true a password prompt should be shown to
   *     the user, and the password should be passed back via a call to
   *     importPersonalCertificatePasswordSelected().
   */
  importPersonalCertificate(useHardwareBacked: boolean): Promise<boolean>;

  importPersonalCertificatePasswordSelected(password: string): Promise<void>;

  /**
   * @return A promise firing once the user has selected
   *     the file to be imported, or failing with CertificatesError.
   *     Upon success, a prompt should be shown to the user to specify the
   *     trust levels, and that information should be passed back via a call
   *     to importCaCertificateTrustSelected().
   */
  importCaCertificate(): Promise<string>;

  /**
   * @return A promise firing once the trust level for the imported
   *     certificate has been successfully set. The promise is rejected if an
   *     error occurred with either a CertificatesError or
   *     CertificatesImportError.
   */
  importCaCertificateTrustSelected(
      ssl: boolean, email: boolean, objSign: boolean): Promise<void>;

  /**
   * @return A promise firing once the certificate has been
   *     imported. The promise is rejected if an error occurred, with either
   *     a CertificatesError or CertificatesImportError.
   */
  importServerCertificate(): Promise<void>;
}

export class CertificatesBrowserProxyImpl implements CertificatesBrowserProxy {
  refreshCertificates() {
    chrome.send('refreshCertificates');
  }

  viewCertificate(id: string) {
    chrome.send('viewCertificate', [id]);
  }

  exportCertificate(id: string) {
    chrome.send('exportCertificate', [id]);
  }

  deleteCertificate(id: string) {
    return sendWithPromise('deleteCertificate', id);
  }

  exportPersonalCertificate(id: string) {
    return sendWithPromise('exportPersonalCertificate', id);
  }

  exportPersonalCertificatePasswordSelected(password: string) {
    return sendWithPromise(
        'exportPersonalCertificatePasswordSelected', password);
  }

  importPersonalCertificate(useHardwareBacked: boolean) {
    return sendWithPromise('importPersonalCertificate', useHardwareBacked);
  }

  importPersonalCertificatePasswordSelected(password: string) {
    return sendWithPromise(
        'importPersonalCertificatePasswordSelected', password);
  }

  getCaCertificateTrust(id: string) {
    return sendWithPromise('getCaCertificateTrust', id);
  }

  editCaCertificateTrust(
      id: string, ssl: boolean, email: boolean, objSign: boolean) {
    return sendWithPromise('editCaCertificateTrust', id, ssl, email, objSign);
  }

  importCaCertificateTrustSelected(
      ssl: boolean, email: boolean, objSign: boolean) {
    return sendWithPromise(
        'importCaCertificateTrustSelected', ssl, email, objSign);
  }

  cancelImportExportCertificate() {
    chrome.send('cancelImportExportCertificate');
  }

  importCaCertificate() {
    return sendWithPromise('importCaCertificate');
  }

  importServerCertificate() {
    return sendWithPromise('importServerCertificate');
  }

  static getInstance(): CertificatesBrowserProxy {
    return instance || (instance = new CertificatesBrowserProxyImpl());
  }

  static setInstance(obj: CertificatesBrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: CertificatesBrowserProxy|null = null;
