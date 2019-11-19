// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Records metrics for mount events.
 */
class MountMetrics {
  constructor() {
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  /**
   * Event handler called when some volume was mounted or unmounted.
   * @param {chrome.fileManagerPrivate.MountCompletedEvent} event Received
   *     event.
   * @private
   */
  onMountCompleted_(event) {
    if (event.eventType === 'mount') {
      if (event.status === 'success' && event.volumeMetadata) {
        if (event.volumeMetadata.volumeType === 'provided') {
          const providerUmaValue =
              this.getFileSystemProviderForUma(event.volumeMetadata.providerId);
          metrics.recordEnum(
              'FileSystemProviderMounted', providerUmaValue,
              Object.keys(MountMetrics.FileSystemProvidersForUMA).length + 1);
        }
      }
    }
  }

  /**
   * Returns the UMA index for a provided file system type. Returns
   * MountMetrics.FileSystemProvidersForUMA.UNKNOWN for unknown providers.
   * @param {string|undefined} providerId The FSP provider ID.
   * @return {MountMetrics.FileSystemProvidersForUMA}
   */
  getFileSystemProviderForUma(providerId) {
    return MountMetrics.FileSystemProviders[providerId] ||
        MountMetrics.FileSystemProvidersForUMA.UNKNOWN;
  }
}


/**
 * This enum provides a lookup for each file system provider type to the UMA
 * enun FileSystemProviderMountType that can be found in
 * tools/metrics/histograms/enums.xml.
 * These values should never be changed, new entries should be added to the end
 * of the enumeration.
 *
 * @enum {number}
 * @const
 */
MountMetrics.FileSystemProvidersForUMA = {
  UNKNOWN: 0,
  ZIP_UNPACKER: 1,
  FILE_SYSTEM_FOR_DROPBOX: 2,
  FILE_SYSTEM_FOR_ONEDRIVE: 3,
  SFTP_FILE_SYSTEM: 4,
  BOX_FOR_CHROMEOS: 5,
  TED_TALKS: 6,
  WEBDAV_FILE_SYSTEM: 7,
  CLOUD_STORAGE: 8,
  SCAN: 9,
  FILE_SYSTEM_FOR_SMB_CIFS: 10,
  ADD_MY_DOCUMENTS: 11,
  WICKED_GOOD_UNARCHIVER: 12,
  NETWORK_FILE_SHARE_FOR_CHROMEOS: 13,
  LAN_FOLDER: 14,
  ZIP_ARCHIVER: 15,
  SECURE_SHELL_APP: 16,
  NATIVE_NETWORK_SMB: 17,
};
Object.freeze(MountMetrics.FileSystemProvidersForUMA);

/**
 * Enumeration of known FSPs used to map to a UMA enumeration index.
 * All FSPs NOT present in this list will be reported as 'Unknown'.
 *
 * These look like extension ids, but are actually provider ids which may
 * but don't have to be extension ids.
 *
 * @enum {MountMetrics.FileSystemProvidersForUMA<number>}
 */
MountMetrics.FileSystemProviders = {
  oedeeodfidgoollimchfdnbmhcpnklnd:
      MountMetrics.FileSystemProvidersForUMA.ZIP_UNPACKER,
  hlffpaajmfllggclnjppbblobdhokjhe:
      MountMetrics.FileSystemProvidersForUMA.FILE_SYSTEM_FOR_DROPBOX,
  jbfdfcehgafdbfpniaimfbfomafoadgo:
      MountMetrics.FileSystemProvidersForUMA.FILE_SYSTEM_FOR_ONEDRIVE,
  gbheifiifcfekkamhepkeogobihicgmn:
      MountMetrics.FileSystemProvidersForUMA.SFTP_FILE_SYSTEM,
  dikonaebkejmpbpcnnmfaeopkaenicgf:
      MountMetrics.FileSystemProvidersForUMA.BOX_FOR_CHROMEOS,
  iibcngmpkgghccnakicfmgajlkhnohep:
      MountMetrics.FileSystemProvidersForUMA.TED_TALKS,
  hmckflbfniicjijmdoffagjkpnjgbieh:
      MountMetrics.FileSystemProvidersForUMA.WEBDAV_FILE_SYSTEM,
  ibfbhbegfkamboeglpnianlggahglbfi:
      MountMetrics.FileSystemProvidersForUMA.CLOUD_STORAGE,
  pmnllmkmjilbojkpgplbdmckghmaocjh: MountMetrics.FileSystemProvidersForUMA.SCAN,
  mfhnnfciefdpolbelmfkpmhhmlkehbdf:
      MountMetrics.FileSystemProvidersForUMA.FILE_SYSTEM_FOR_SMB_CIFS,
  plmanjiaoflhcilcfdnjeffklbgejmje:
      MountMetrics.FileSystemProvidersForUMA.ADD_MY_DOCUMENTS,
  mljpablpddhocfbnokacjggdbmafjnon:
      MountMetrics.FileSystemProvidersForUMA.WICKED_GOOD_UNARCHIVER,
  ndjpildffkeodjdaeebdhnncfhopkajk:
      MountMetrics.FileSystemProvidersForUMA.NETWORK_FILE_SHARE_FOR_CHROMEOS,
  gmhmnhjihabohahcllfgjooaoecglhpi:
      MountMetrics.FileSystemProvidersForUMA.LAN_FOLDER,
  dmboannefpncccogfdikhmhpmdnddgoe:
      MountMetrics.FileSystemProvidersForUMA.ZIP_ARCHIVER,
  pnhechapfaindjhompbnflcldabbghjo:
      MountMetrics.FileSystemProvidersForUMA.SECURE_SHELL_APP,
  /**
   * Native Providers.
   */
  '@smb': MountMetrics.FileSystemProvidersForUMA.NATIVE_NETWORK_SMB,
};
Object.freeze(MountMetrics.FileSystemProviders);
