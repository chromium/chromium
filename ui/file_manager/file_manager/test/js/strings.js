// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// loadTimeData contains localized content.  It is populated with
// file_manager_strings.grdp and chromeos_strings.grdp during build.

loadTimeData.data = $GRDP;

// Extend with additional fields not found in grdp files.
loadTimeData.overrideValues({
  'CROSTINI_ENABLED': true,
  'DRIVE_BIDIRECTIONAL_NATIVE_MESSAGING_ENABLED': false,
  'FILES_NG_ENABLED': true,
  'FILES_SINGLE_PARTITION_FORMAT_ENABLED': false,
  'FILES_TRANSFER_DETAILS_ENABLED': true,
  'FILTERS_IN_RECENTS_ENABLED': false,
  'FEEDBACK_PANEL_ENABLED': false,
  'GOOGLE_DRIVE_REDEEM_URL': 'http://www.google.com/intl/en/chrome/devices' +
      '/goodies.html?utm_source=filesapp&utm_medium=banner&utm_campaign=gsg',
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
  'language': 'en-US',
  'textdirection': 'ltr',
});

// Overwrite LoadTimeData.prototype.data setter as nop.
// Default implementation throws errors when both background and
// foreground re-set loadTimeData.data.
Object.defineProperty(LoadTimeData.prototype, 'data', {set: () => {}});
