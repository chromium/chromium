// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as androidPhotosTests from './file_manager/android_photos.js';
// clang-format off
import * as breadcrumbsTests from './file_manager/breadcrumbs.js';
import * as contextMenuTests from './file_manager/context_menu.js';
import * as copyBetweenWindowsTests from './file_manager/copy_between_windows.js';
import * as createNewFolderTests from './file_manager/create_new_folder.js';
import * as crostiniTests from './file_manager/crostini.js';
import * as directoryTreeTests from './file_manager/directory_tree.js';
import * as directoryTreeContextMenuTests from './file_manager/directory_tree_context_menu.js';
import * as dlpTests from './file_manager/dlp.js';
import * as dlpEnterpriseConnectorsTests from './file_manager/dlp_enterprise_connectors.js';
import * as driveSpecificTests from './file_manager/drive_specific.js';
import * as fileDialogTests from './file_manager/file_dialog.js';
import * as fileDisplayTests from './file_manager/file_display.js';
import * as fileListTests from './file_manager/file_list.js';
import * as fileTransferConnectorTests from './file_manager/file_transfer_connector.js';
import * as filesTooltipTests from './file_manager/files_tooltip.js';
import * as folderShortcutsTests from './file_manager/folder_shortcuts.js';
import * as formatDialogTests from './file_manager/format_dialog.js';
import * as gearMenuTests from './file_manager/gear_menu.js';
import * as gridViewTests from './file_manager/grid_view.js';
import * as guestOsTests from './file_manager/guest_os.js';
import * as holdingSpaceTests from './file_manager/holding_space.js';
import * as installLinuxPackageDialogTests from './file_manager/install_linux_package_dialog.js';
import * as keyboardOperationsTests from './file_manager/keyboard_operations.js';
import * as manageDialogTests from './file_manager/manage_dialog.js';
import * as materializedViewsTests from './file_manager/materialized_views.js';
import * as metadataTests from './file_manager/metadata.js';
import * as metricsTests from './file_manager/metrics.js';
import * as myFilesTests from './file_manager/my_files.js';
import * as navigationTests from './file_manager/navigation.js';
import * as officeTests from './file_manager/office.js';
import * as openAudioMediaAppTests from './file_manager/open_audio_media_app.js';
import * as openFilesInWebDriveTests from './file_manager/open_files_in_web_drive.js';
import * as openImageMediaAppTests from './file_manager/open_image_media_app.js';
import * as openSniffedFilesTests from './file_manager/open_sniffed_files.js';
import * as openVideoMediaAppTests from './file_manager/open_video_media_app.js';
import * as providersTests from './file_manager/providers.js';
import * as quickViewTests from './file_manager/quick_view.js';
import * as recentsTests from './file_manager/recents.js';
import * as restorePrefsTests from './file_manager/restore_prefs.js';
import * as searchTests from './file_manager/search.js';
import * as shareTests from './file_manager/share.js';
import * as sortColumnsTests from './file_manager/sort_columns.js';
import * as tabIndexTests from './file_manager/tab_index.js';
import * as tasksTests from './file_manager/tasks.js';
import * as toolbarTests from './file_manager/toolbar.js';
import * as transferTests from './file_manager/transfer.js';
import * as trashTests from './file_manager/trash.js';
import * as traverseTests from './file_manager/traverse.js';
import * as zipFilesTests from './file_manager/zip_files.js';
import * as skyVaultTests from './file_manager/skyvault.js';
// clang-format on

export type TestFunctionName = string;
export type TestFunction = () => Promise<void|any>;

/**
 * Namespace for test cases.
 */
export const testcase: Record<TestFunctionName, TestFunction> = {
  ...androidPhotosTests,
  ...breadcrumbsTests,
  ...contextMenuTests,
  ...copyBetweenWindowsTests,
  ...createNewFolderTests,
  ...crostiniTests,
  ...directoryTreeTests,
  ...directoryTreeContextMenuTests,
  ...dlpTests,
  ...dlpEnterpriseConnectorsTests,
  ...driveSpecificTests,
  ...fileDialogTests,
  ...fileDisplayTests,
  ...fileListTests,
  ...fileTransferConnectorTests,
  ...filesTooltipTests,
  ...folderShortcutsTests,
  ...formatDialogTests,
  ...gearMenuTests,
  ...gridViewTests,
  ...guestOsTests,
  ...holdingSpaceTests,
  ...installLinuxPackageDialogTests,
  ...keyboardOperationsTests,
  ...manageDialogTests,
  ...metadataTests,
  ...metricsTests,
  ...materializedViewsTests,
  ...myFilesTests,
  ...navigationTests,
  ...officeTests,
  ...openAudioMediaAppTests,
  ...openFilesInWebDriveTests,
  ...openImageMediaAppTests,
  ...openSniffedFilesTests,
  ...openVideoMediaAppTests,
  ...providersTests,
  ...quickViewTests,
  ...recentsTests,
  ...restorePrefsTests,
  ...searchTests,
  ...shareTests,
  ...sortColumnsTests,
  ...tabIndexTests,
  ...tasksTests,
  ...toolbarTests,
  ...transferTests,
  ...trashTests,
  ...traverseTests,
  ...zipFilesTests,
  ...skyVaultTests,
};
