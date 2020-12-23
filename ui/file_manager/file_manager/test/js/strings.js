// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// loadTimeData contains localized content.  It is populated with
// file_manager_strings.grdp and chromeos_strings.grdp during build.

loadTimeData.data = $GRDP;

// Extend with additional fields not found in grdp files.
loadTimeData.overrideValues({
  'COPY_IMAGE_ENABLED': false,
  'CROSTINI_ENABLED': true,
  'FILES_CAMERA_FOLDER_ENABLED': false,
  'FILES_NG_ENABLED': true,
  'FILES_SINGLE_PARTITION_FORMAT_ENABLED': false,
  'FILES_TRASH_ENABLED': true,
  'FILTERS_IN_RECENTS_ENABLED': false,
  'GOOGLE_DRIVE_OVERVIEW_URL':
      'https://support.google.com/chromebook/?p=filemanager_drive',
  'HIDE_SPACE_INFO': false,
  'ARC_USB_STORAGE_UI_ENABLED': true,
  'PLUGIN_VM_ENABLED': true,
  'UNIFIED_MEDIA_VIEW_ENABLED': false,
  'UI_LOCALE': 'en_US',
  'ZIP_MOUNT': false,
  'ZIP_PACK': false,
  'ZIP_UNPACK': false,
});

// Overwrite LoadTimeData.prototype.data setter as nop.
// Default implementation throws errors when both background and
// foreground re-set loadTimeData.data.
Object.defineProperty(LoadTimeData.prototype, 'data', {set: () => {}});
