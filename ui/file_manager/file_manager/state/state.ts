// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../common/js/files_app_entry_types.js';
import type {DialogType} from '../common/js/shared_types.js';
import type {RootType, VolumeType} from '../common/js/volume_manager_types.js';
import type {MetadataItem} from '../foreground/js/metadata/metadata_item.js';

export {DialogType} from '../common/js/shared_types.js';

export enum EntryType {
  // Entries from the FileSystem API.
  FS_API = 'FS_API',

  // The root of a volume is an Entry from the FileSystem API, but it aggregates
  // more data from the volume.
  VOLUME_ROOT = 'VOLUME_ROOT',

  // A directory-like entry to aggregate other entries.
  ENTRY_LIST = 'ENTRY_LIST',

  // Placeholder that is replaced for another entry, for Crostini/GuestOS.
  PLACEHOLDER = 'PLACEHOLDER',

  // Root for the Trash.
  TRASH = 'TRASH',

  // Root for the Recent.
  RECENT = 'RECENT',

  // A folder-like that doesn't have an entry linked to it.
  MATERIALIZED_VIEW = 'MATERIALIZED_VIEW',
}

/**
 * The data for each individual file/entry.
 */
export interface FileData {
  /** `key` is the file URL. */
  key: FileKey;
  fullPath: string;
  entry?: Entry|FilesAppEntry;

  /**
   * `icon` can be either a string or a IconSet which is an object including
   * both high/low DPI icon data.
   */
  icon: string|chrome.fileManagerPrivate.IconSet;
  label: string;
  volumeId: VolumeId|null;
  rootType: RootType|null;
  metadata: MetadataItem;
  isDirectory: boolean;
  type: EntryType;
  isRootEntry: boolean;
  isEjectable: boolean;
  canExpand: boolean;

  /**
   * TODO(b/271485133): `children` here only store sub directories for now, it
   * should store all children including files, it's up to the container to do
   * filter and sorting if needed.
   */
  children: FileKey[];
  expanded: boolean;
  disabled: boolean;
}

/**
 * A stronger type for identifying an entry.
 * Currently entry is identified by its URL as string.
 *
 * NOTE: Fake entry, as in, entries implemented directly in JS/TS are identified
 * by a made up URL like:
 * fake-entry://recent
 * fake-entry://trash
 */
export type FileKey = string;

/**
 * A stronger type for identifying a volume.
 */
export type VolumeId = string;

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 */
export interface PathComponent {
  name: string;
  label: string;
  key: FileKey;
}

/**
 * The status of a property, for properties that have their state updated via
 * asynchronous steps.
 */
export enum PropStatus {
  STARTED = 'STARTED',

  // Finished:
  SUCCESS = 'SUCCESS',
  ERROR = 'ERROR',
}

/**
 * Task type is the source of the task, or what type of the app is this type
 * from. It has to match the `taskType` returned in the FileManagerPrivate.
 *
 * For more details see //chrome/browser/ash/file_manager/file_tasks.h
 */
export enum FileTaskType {
  UNKNOWN = '',
  // The task is from a chrome app/extension that has File Browser Handler in
  // its manifest.
  FILE = 'file',
  // The task is from a chrome app/extension that has File Handler in its
  // manifest.
  APP = 'app',
  // The task is from an Android app.
  ARC = 'arc',
  // The task is from a Crostini app.
  CROSTINI = 'crostini',
  // The task is from a Parallels app.
  PLUGIN_VM = 'pluginvm',
  // The task is from a Web app/PWA/SWA.
  WEB = 'web',
}

/**
 * Task Descriptor it's the unique identified for a Task.
 *
 * For more details see //chrome/browser/ash/file_manager/file_tasks.h
 */
export interface FileTaskDescriptor {
  appId: string;
  taskType: FileTaskType;
  actionId: string;
}

/**
 * UI representation for File Task.
 * NOTE: This is slightly different from the FileTask from the
 * FileManagerPrivate API. Here the task is enhanced to deal with different
 * displaying icons and labels.
 * TODO(lucmult): Change isDefault and isGenericFileHandler to boolean when
 * non-Store version doesn't have to be supported anymore.
 */
export interface FileTask {
  descriptor: FileTaskDescriptor;
  title: string;
  iconUrl: string|undefined;
  iconType: string;
  isDefault: boolean|undefined;
  isGenericFileHandler: boolean|undefined;
  isDlpBlocked: boolean|undefined;
}

/**
 * Container for FileTask.
 * `defaultHandlerPolicy` is only set if the user can't change the default
 * handler due to a policy, either the policy forces the default handler or
 * the policy is incorrect, but we still don't allow user to change the
 * default.
 *
 * TODO(lucmult): keys might not be needed here.
 */
export interface FileTasks {
  tasks: FileTask[];
  policyDefaultHandlerStatus:
      chrome.fileManagerPrivate.PolicyDefaultHandlerStatus|undefined;
  defaultTask: (FileTask|undefined);
  status: PropStatus;
}

/**
 * Launch parameters for the file manager.
 */
export interface LaunchParams {
  dialogType: DialogType|undefined;
}

/**
 * This represents the entries currently selected, out of the entries
 * displayed in the file list/grid.
 */
export interface Selection {
  keys: FileKey[];
  dirCount: number;
  fileCount: number;
  hostedCount: number|undefined;
  offlineCachedCount: number;
  fileTasks: FileTasks;
}

/**
 * Represents the entries displayed in the file list/grid.
 */
export interface DirectoryContent {
  status: PropStatus;
  keys: FileKey[];
}

/**
 * The current directory.
 * The directory is only effectively active when the `status` is SUCCESS.
 */
export interface CurrentDirectory {
  status: PropStatus;
  key: FileKey;
  pathComponents: PathComponent[];
  content: DirectoryContent;
  selection: Selection;
  rootType: RootType|undefined;
  hasDlpDisabledFiles: boolean;
}

/**
 * Enumeration of all supported search locations. If new location is added,
 * please update this enum.
 */
export enum SearchLocation {
  EVERYWHERE = 'everywhere',
  ROOT_FOLDER = 'root_folder',
  THIS_FOLDER = 'this_folder',
}

/**
 * Enumeration of all supported how-recent time spans.
 */
export enum SearchRecency {
  ANYTIME = 'anytime',
  TODAY = 'today',
  YESTERDAY = 'yesterday',
  LAST_WEEK = 'last_week',
  LAST_MONTH = 'last_month',
  LAST_YEAR = 'last_year',
}

/**
 * The options used by the file search operation.
 */
export interface SearchOptions {
  location: SearchLocation;
  recency: SearchRecency;
  fileCategory: chrome.fileManagerPrivate.FileCategory;
}

/**
 * Data for search. It should be empty `{}` when the user isn't searching.
 */
export interface SearchData {
  status: PropStatus|undefined;
  query: string|undefined;
  options: SearchOptions|undefined;
}

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
 */
export enum NavigationSection {
  TOP = 'top',
  MY_FILES = 'my_files',
  GOOGLE_DRIVE = 'google_drive',
  ODFS = 'odfs',
  CLOUD = 'cloud',
  TRASH = 'trash',
  ANDROID_APPS = 'android_apps',
  REMOVABLE = 'removable',
}

export enum NavigationType {
  SHORTCUT = 'shortcut',
  VOLUME = 'volume',
  RECENT = 'recent',
  CROSTINI = 'crostini',
  GUEST_OS = 'guest_os',
  ENTRY_LIST = 'entry_list',
  DRIVE = 'drive',
  ANDROID_APPS = 'android_apps',
  TRASH = 'trash',
  // Materialized view is used for Recent and in the future for Search.
  MATERIALIZED_VIEW = 'materialized_view',
}

/**
 * The key of navigation item, it could be:
 *   * FileKey: the navigation is backed up by a real file entry.
 *   * string: the navigation is backed up by others (e.g. androids_apps).
 */
export type NavigationKey = FileKey|string;

/**
 * This represents the navigation root node, it can be backed up by an file
 * entry or an Android app package (e.g. for android_apps type). If its type
 * is android_apps, the `key` filed will be android app's package name, not a
 * file key.
 */
export interface NavigationRoot {
  key: NavigationKey;
  section: NavigationSection;
  type: NavigationType;
  separator: boolean;
}

export interface NavigationTree {
  /** It's just a ordered array with NavigationRoot. */
  roots: NavigationRoot[];
}

/**
 * This carries the same information as VolumeInfo, which is very similar to
 * fileManagerPrivate.VolumeMetadata.
 *
 * The property names are identical to VolumeInfo to simplify the migration.
 * Notable differences: missing the properties: profile, remoteMountPath.
 *
 * When the volume has an unrecognized file system, it's still mounted here,
 * but with `error`=="unknown".
 */
export interface Volume {
  volumeId: VolumeId;
  volumeType: VolumeType;
  rootKey: FileKey|undefined;
  status: PropStatus;
  label: string;
  error: string|undefined;
  deviceType: chrome.fileManagerPrivate.DeviceType|undefined;
  devicePath: string|undefined;
  isReadOnly: boolean;
  isReadOnlyRemovableDevice: boolean;
  providerId: string|undefined;
  configurable: boolean;
  watchable: boolean;
  source: chrome.fileManagerPrivate.Source|undefined;
  diskFileSystemType: (string|undefined);
  iconSet: chrome.fileManagerPrivate.IconSet|undefined;
  driveLabel: string|undefined;
  vmType: chrome.fileManagerPrivate.VmType|undefined;
  isDisabled: boolean;
  prefixKey: FileKey|undefined;
  isInteractive: boolean;
}

export type VolumeMap = Record<VolumeId, Volume>;

/**
 * This carries the state related to physical user device.
 */
export interface Device {
  connection: chrome.fileManagerPrivate.DeviceConnectionState;
}

/**
 * This carries the state related to the underlying Drive connection status.
 * This differs from the device connection state as the Drive can also be in a
 * effectively paused state when on a metered network.
 */
export interface Drive {
  connectionType: chrome.fileManagerPrivate.DriveConnectionStateType;
  offlineReason: chrome.fileManagerPrivate.DriveOfflineReason|undefined;
}

/**
 * An extension of `chrome.fileManagerPrivate.AndroidApp`. The only difference
 * from the private API AndroidApp is this one adds an union type `icon`. This
 * is because `iconSet` can generate a "none" background sometimes, in this
 * case we need a backup icon instead.
 *
 * Note: we keep `iconSet` here to be compatible with the original AndroidApp
 * type because private API `selectAndroidPickerApp` still requires the
 * original type. For other use cases, we can ignore `iconSet` and just use
 * `icon`.
 */
export interface AndroidApp {
  name: string;
  packageName: string;
  activityName: string;
  iconSet?: chrome.fileManagerPrivate.IconSet|undefined;
  icon: string|chrome.fileManagerPrivate.IconSet;
}

/**
 * A view behaves like a folder, as in, it's a collection of FileData.
 *
 * Its content comes from the File Index.
 */
export interface MaterializedView {
  id: string;
  key: FileKey;
  label: string;
  icon: string;
  isRoot: boolean;
}

/**
 * Files app's state.
 */
export interface State {
  allEntries: Record<FileKey, FileData>;
  currentDirectory: CurrentDirectory|undefined;
  device: Device;
  drive: Drive;
  launchParams: LaunchParams;
  search: SearchData|undefined;
  navigation: NavigationTree;
  volumes: Record<VolumeId, Volume>;
  uiEntries: FileKey[];
  folderShortcuts: FileKey[];
  androidApps: Record<string, AndroidApp>;
  bulkPinning?: chrome.fileManagerPrivate.BulkPinProgress;
  preferences?: chrome.fileManagerPrivate.Preferences;
  materializedViews: MaterializedView[];
}
