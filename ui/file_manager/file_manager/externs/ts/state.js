// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {FilesAppEntry} from '../files_app_entry_interfaces.js';

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 * @enum {string}
 */
export const DialogType = {
  SELECT_FOLDER: 'folder',
  SELECT_UPLOAD_FOLDER: 'upload-folder',
  SELECT_SAVEAS_FILE: 'saveas-file',
  SELECT_OPEN_FILE: 'open-file',
  SELECT_OPEN_MULTI_FILE: 'open-multi-file',
  FULL_PAGE: 'full-page',
};

/**
 * @enum {string}
 */
export const EntryType = {
  // Entries from the FileSystem API.
  FS_API: 'FS_API',

  // The root of a volume is an Entry from the FileSystem API, but it aggregates
  // more data from the volume.
  VOLUME_ROOT: 'VOLUME_ROOT',

  // A directory-like entry to aggregate other entries.
  ENTRY_LIST: 'ENTRY_LIST',

  // Placeholder that is replaced for another entry, for Crostini/GuestOS.
  PLACEHOLDER: 'PLACEHOLDER',

  // Root for the Trash.
  TRASH: 'TRASH',

  // Root for the Recent.
  RECENT: 'RECENT',
};

/**
 * The data for each individual file/entry.
 *
 * * `icon` can be either a string or a IconSet which is an object including
 * both high/low DPI icon data.
 *
 * TODO(b/271485133): `children` here only store sub directories for now, it
 * should store all children including files, it's up to the container to do
 * filter and sorting if needed.
 *
 * @typedef {{
 *   entry: (Entry|FilesAppEntry),
 *   icon: (string|chrome.fileManagerPrivate.IconSet),
 *   label: string,
 *   volumeId: (VolumeId|null),
 *   rootType: (RootType|null),
 *   metadata: MetadataItem,
 *   isDirectory: boolean,
 *   type: EntryType,
 *   isRootEntry: boolean,
 *   isEjectable: boolean,
 *   shouldDelayLoadingChildren: boolean,
 *   children: (Array<FileKey>),
 *   expanded: boolean,
 *   disabled: boolean,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'FileData' implicitly has an 'any' type.
export let FileData;

/**
 * A stronger type for identifying an entry.
 * Currently entry is identified by its URL as string.
 *
 * NOTE: Fake entry, as in, entries implemented directly in JS/TS are identified
 * by a made up URL like:
 * fake-entry://recent
 * fake-entry://trash
 *
 * @typedef {string}
 */
// @ts-ignore: error TS7005: Variable 'FileKey' implicitly has an 'any' type.
export let FileKey;

/**
 * A stronger type for identifying a volume.
 * @typedef {string}
 */
// @ts-ignore: error TS7005: Variable 'VolumeId' implicitly has an 'any' type.
export let VolumeId;

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 * @typedef {{
 *   name: string,
 *   label: string,
 *   key: FileKey,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'PathComponent' implicitly has an 'any'
// type.
export let PathComponent;

/**
 * The status of a property, for properties that have their state updated via
 * asynchronous steps.
 * @enum {string}
 */
export const PropStatus = {
  STARTED: 'STARTED',

  // Finished:
  SUCCESS: 'SUCCESS',
  ERROR: 'ERROR',
};

/**
 * Task type is the source of the task, or what type of the app is this type
 * from. It has to match the `taskType` returned in the FileManagerPrivate.
 *
 * For more details see //chrome/browser/ash/file_manager/file_tasks.h
 * @enum {string}
 */
export const FileTaskType = {
  UNKNOWN: '',
  // The task is from a chrome app/extension that has File Browser Handler in
  // its manifest.
  FILE: 'file',
  // The task is from a chrome app/extension that has File Handler in its
  // manifest.
  APP: 'app',
  // The task is from an Android app.
  ARC: 'arc',
  // The task is from a Crostini app.
  CROSTINI: 'crostini',
  // The task is from a Parallels app.
  PLUGIN_VM: 'pluginvm',
  // The task is from a Web app/PWA/SWA.
  WEB: 'web',
};

/**
 * Task Descriptor it's the unique identified for a Task.
 *
 * For more details see //chrome/browser/ash/file_manager/file_tasks.h
 * @typedef {{
 *   appId: string,
 *   taskType: FileTaskType,
 *   actionId: string,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'FileTaskDescriptor' implicitly has an
// 'any' type.
export let FileTaskDescriptor;

/**
 * UI representation for File Task.
 * NOTE: This is slightly different from the FileTask from the
 * FileManagerPrivate API. Here the task is enhanced to deal with different
 * displaying icons and labels.
 * TODO(lucmult): Change isDefault and isGenericFileHandler to boolean when
 * non-Store version doesn't have to be supported anymore.
 *
 * @typedef {{
 *   descriptor: FileTaskDescriptor,
 *   title: string,
 *   iconUrl: (string|undefined),
 *   iconType: string,
 *   isDefault: (boolean|undefined),
 *   isGenericFileHandler: (boolean|undefined),
 *   isDlpBlocked: (boolean|undefined),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'FileTask' implicitly has an 'any' type.
export let FileTask;

/**
 * Container for FileTask.
 * `defaultHandlerPolicy` is only set if the user can't change the default
 * handler due to a policy, either the policy forces the default handler or the
 * policy is incorrect, but we still don't allow user to change the default.
 *
 * TODO(lucmult): keys might not be needed here.
 *
 * @typedef {{
 *   tasks: Array<FileTask>,
 *   policyDefaultHandlerStatus:
 *      (chrome.fileManagerPrivate.PolicyDefaultHandlerStatus|undefined),
 *   defaultTask: (FileTask|undefined),
 *   status: PropStatus,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'FileTasks' implicitly has an 'any' type.
export let FileTasks;

/**
 * Launch parameters for the file manager.
 * @typedef {{
 *   dialogType: (DialogType|undefined),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'LaunchParams' implicitly has an 'any'
// type.
export let LaunchParams;

/**
 * This represents the entries currently selected, out of the entries displayed
 * in the file list/grid.
 *
 * @typedef {{
 *    keys: Array<FileKey>,
 *    dirCount: number,
 *    fileCount: number,
 *    hostedCount: (number|undefined),
 *    offlineCachedCount: number,
 *    fileTasks: FileTasks,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'Selection' implicitly has an 'any' type.
export let Selection;

/**
 * Represents the entries displayed in the file list/grid.
 *
 * @typedef {{
 *    keys: Array<FileKey>,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'DirectoryContent' implicitly has an 'any'
// type.
export let DirectoryContent;

/**
 * The current directory.
 * The directory is only effectively active when the `status` is SUCCESS.
 * @typedef {{
 *   status: PropStatus,
 *   key: FileKey,
 *   pathComponents: Array<PathComponent>,
 *   content: DirectoryContent,
 *   selection: Selection,
 *   rootType: (RootType|undefined),
 *   hasDlpDisabledFiles: boolean,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'CurrentDirectory' implicitly has an 'any'
// type.
export let CurrentDirectory;

/**
 * Enumeration of all supported search locations. If new location is added,
 * please update this enum.
 * @enum {string}
 */
export const SearchLocation = {
  EVERYWHERE: 'everywhere',
  ROOT_FOLDER: 'root_folder',
  THIS_FOLDER: 'this_folder',
};

/**
 * Enumeration of all supported how-recent time spans.
 * @enum{string}
 */
export const SearchRecency = {
  ANYTIME: 'anytime',
  TODAY: 'today',
  YESTERDAY: 'yesterday',
  LAST_WEEK: 'last_week',
  LAST_MONTH: 'last_month',
  LAST_YEAR: 'last_year',
};

/**
 * The options used by the file search operation.
 * @typedef {{
 *   location: SearchLocation,
 *   recency: SearchRecency,
 *   fileCategory:  chrome.fileManagerPrivate.FileCategory,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'SearchOptions' implicitly has an 'any'
// type.
export let SearchOptions;

/**
 * Data for search. It should be empty `{}` when the user isn't searching.
 * @typedef {{
 *   status: (PropStatus|undefined),
 *   query: (string|undefined),
 *   options: (SearchOptions|undefined),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'SearchData' implicitly has an 'any' type.
export let SearchData;

/**
 * Used to group volumes in the navigation tree.
 * Sections:
 *      - TOP: Recents, Shortcuts.
 *      - MY_FILES: My Files (which includes Downloads, Crostini and Arc++ as
 *                  its children).
 *      - TRASH: trash.
 *      - GOOGLE_DRIVE: Just Google Drive.
 *      - ODFS: Just ODFS.
 *      - CLOUD: All other cloud: SMBs, FSPs and Documents Providers.
 *      - ANDROID_APPS: ANDROID picker apps.
 *      - REMOVABLE: Archives, MTPs, Media Views and Removables.
 * @enum {string}
 */
export const NavigationSection = {
  TOP: 'top',
  MY_FILES: 'my_files',
  GOOGLE_DRIVE: 'google_drive',
  ODFS: 'odfs',
  CLOUD: 'cloud',
  TRASH: 'trash',
  ANDROID_APPS: 'android_apps',
  REMOVABLE: 'removable',
};

/**
 * @enum {string}
 */
export const NavigationType = {
  SHORTCUT: 'shortcut',
  VOLUME: 'volume',
  RECENT: 'recent',
  CROSTINI: 'crostini',
  GUEST_OS: 'guest_os',
  ENTRY_LIST: 'entry_list',
  DRIVE: 'drive',
  ANDROID_APPS: 'android_apps',
  TRASH: 'trash',
  // Materialized view is used for Recent and in the future for Search.
  MATERIALIZED_VIEW: 'materialized_view',
};

/**
 * The key of navigation item, it could be:
 *   * FileKey: the navigation is backed up by a real file entry.
 *   * string: the navigation is backed up by others (e.g. androids_apps).
 * @typedef {FileKey|string}
 */
// @ts-ignore: error TS7005: Variable 'NavigationKey' implicitly has an 'any'
// type.
export let NavigationKey;

/**
 * This represents the navigation root node, it can be backed up by an file
 * entry or an Android app package (e.g. for android_apps type). If its type
 * is android_apps, the `key` filed will be android app's package name, not a
 * file key.
 *
 * @typedef {{
 *   key: NavigationKey,
 *   section: NavigationSection,
 *   type: NavigationType,
 *   separator: boolean,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'NavigationRoot' implicitly has an 'any'
// type.
export let NavigationRoot;

/**
 * `roots` is just a ordered array with NavigationRoot.
 * @typedef {{
 *   roots: Array<NavigationRoot>,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'NavigationTree' implicitly has an 'any'
// type.
export let NavigationTree;

/**
 * This carries the same information as VolumeInfo, which is very similar to
 * fileManagerPrivate.VolumeMetadata.
 *
 * The property names are identical to VolumeInfo to simplify the migration.
 * Notable differences: missing the properties: profile, hasMedia,
 * remoteMountPath.
 *
 * When the volume has an unrecognized file system, it's still mounted here, but
 * with `error`=="unknown".
 *
 * @typedef {{
 *   volumeId: VolumeId,
 *   volumeType: VolumeType,
 *   rootKey: (FileKey|undefined),
 *   status: PropStatus,
 *   label: string,
 *   error: (string|undefined),
 *   deviceType: (chrome.fileManagerPrivate.DeviceType|undefined),
 *   devicePath: (string|undefined),
 *   isReadOnly: boolean,
 *   isReadOnlyRemovableDevice: boolean,
 *   providerId: (string|undefined),
 *   configurable: boolean,
 *   watchable: boolean,
 *   source: (chrome.fileManagerPrivate.Source|undefined),
 *   diskFileSystemType: (string|undefined),
 *   iconSet: (chrome.fileManagerPrivate.IconSet|undefined),
 *   driveLabel: (string|undefined),
 *   vmType: (chrome.fileManagerPrivate.VmType|undefined),
 *   isDisabled: boolean,
 *   prefixKey: (FileKey|undefined),
 *   isInteractive: boolean,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'Volume' implicitly has an 'any' type.
export let Volume;

/**
 * @typedef {Record<VolumeId, Volume>}
 */
// @ts-ignore: error TS7005: Variable 'VolumeMap' implicitly has an 'any' type.
export let VolumeMap;

/**
 * This carries the state related to physical user device.
 *
 * @typedef {{
 *   connection: (chrome.fileManagerPrivate.DeviceConnectionState),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'Device' implicitly has an 'any' type.
export let Device;

/**
 * This carries the state related to the underlying Drive connection status.
 * This differs from the device connection state as the Drive can also be in a
 * effectively paused state when on a metered network.
 *
 * @typedef {{
 *   connectionType: (chrome.fileManagerPrivate.DriveConnectionStateType),
 *   offlineReason: (chrome.fileManagerPrivate.DriveOfflineReason|undefined),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'Drive' implicitly has an 'any' type.
export let Drive;

/**
 * An extension of `chrome.fileManagerPrivate.AndroidApp`. The only difference
 * from the private API AndroidApp is this one adds an union type `icon`. This
 * is because `iconSet` can generate a "none" background sometimes, in this case
 * we need a backup icon instead.
 *
 * Note: we keep `iconSet` here to be compatible with the original AndroidApp
 * type because private API `selectAndroidPickerApp` still requires the original
 * type. For other use cases, we can ignore `iconSet` and just use `icon`.
 *
 * @typedef {{
 *   name: string,
 *   packageName: string,
 *   activityName: string,
 *   iconSet: (chrome.fileManagerPrivate.IconSet|undefined),
 *   icon: (string|chrome.fileManagerPrivate.IconSet),
 * }}
 */
// @ts-ignore: error TS7005: Variable 'AndroidApp' implicitly has an 'any' type.
export let AndroidApp;

/**
 * Files app's state.
 * @typedef {{
 *   allEntries: Record<FileKey, FileData>,
 *   currentDirectory: (CurrentDirectory|undefined),
 *   device: Device,
 *   drive: Drive,
 *   launchParams: !LaunchParams,
 *   search: (SearchData|undefined),
 *   navigation: NavigationTree,
 *   volumes: Record<VolumeId, Volume>,
 *   uiEntries: Array<FileKey>,
 *   folderShortcuts: Array<FileKey>,
 *   androidApps: Record<string, AndroidApp>,
 *   bulkPinning?: chrome.fileManagerPrivate.BulkPinProgress,
 *   preferences?: chrome.fileManagerPrivate.Preferences,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'State' implicitly has an 'any' type.
export let State;
