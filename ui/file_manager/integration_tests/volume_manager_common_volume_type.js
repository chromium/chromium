// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/199452030): Fix duplication with volume_manager_types.js
/**
 * The type of each volume.
 * @enum {string}
 * @const
 */
export const VolumeManagerCommonVolumeType = {
  DRIVE: 'drive',
  DOWNLOADS: 'downloads',
  REMOVABLE: 'removable',
  ARCHIVE: 'archive',
  MTP: 'mtp',
  PROVIDED: 'provided',
  MEDIA_VIEW: 'media_view',
  DOCUMENTS_PROVIDER: 'documents_provider',
  CROSTINI: 'crostini',
  ANDROID_FILES: 'android_files',
  MY_FILES: 'my_files',
  SMB: 'smb',
  TRASH: 'trash',
};
