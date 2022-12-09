// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {FilesAppEntry} from '../files_app_entry_interfaces.js';

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
 * @typedef {{
 *   entry: (Entry|FilesAppEntry),
 *   iconName: (string|undefined),
 *   label: string,
 *   volumeType: (VolumeManagerCommon.VolumeType|null),
 *   metadata: !MetadataItem,
 *   isDirectory: boolean,
 *   type: !EntryType,
 * }}
 */
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
export let FileKey;

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 * @typedef {{
 *   name: string,
 *   label: string,
 *   key: !FileKey,
 * }}
 */
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
 *   taskType: !FileTaskType,
 *   actionId: string,
 * }}
 */
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
 *   descriptor: !FileTaskDescriptor,
 *   title: string,
 *   iconUrl: (string|undefined),
 *   iconType: string,
 *   isDefault: (boolean|undefined),
 *   isGenericFileHandler: (boolean|undefined),
 * }}
 */
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
 *   tasks: !Array<!FileTask>,
 *   policyDefaultHandlerStatus:
 *      (chrome.fileManagerPrivate.PolicyDefaultHandlerStatus|undefined),
 *   defaultTask: (FileTask|undefined),
 *   status: !PropStatus,
 * }}
 */
export let FileTasks;

/**
 * This represents the entries currently selected, out of the entries displayed
 * in the file list/grid.
 *
 * @typedef {{
 *    keys: !Array<!FileKey>,
 *    dirCount: number,
 *    fileCount: number,
 *    hostedCount: (number|undefined),
 *    offlineCachedCount: (number|undefined),
 *    fileTasks: !FileTasks,
 * }}
 */
export let Selection;

/**
 * The current directory.
 * The directory is only effectively active when the `status` is SUCCESS.
 * @typedef {{
 *   status: !PropStatus,
 *   key: !FileKey,
 *   pathComponents: !Array<PathComponent>,
 *   selection: Selection,
 *   rootType: (VolumeManagerCommon.RootType|undefined),
 * }}
 */
export let CurrentDirectory;

/**
 * Enumeration of all supported search locations. If new location is added,
 * please update this enum.
 * @enum {string}
 */
export const SearchLocation = {
  EVERYWHERE: 'everywhere',
  THIS_CHROMEBOOK: 'this_chromebook',
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
 * Enumeration of all supported file types. We use generic buckets such as
 * Images, to denote all "*.jpg", "*.gif", "*.png", etc., file types.
 * @enum{string}
 */
export const SearchFileType = {
  ALL_TYPES: 'all_types',
  AUDIO: 'audio',
  DOCUMENTS: 'documents',
  IMAGES: 'images',
  VIDEOS: 'videos',
};

/**
 * The options used by the file search operation.
 * @typedef {{
 *   location: SearchLocation,
 *   recency: SearchRecency,
 *   type: SearchFileType,
 * }}
 */
export let SearchOptions;

/**
 * Data for search. It should be empty `{}` when the user isn't searching.
 * @typedef {{
 *   status: (PropStatus|undefined),
 *   query: (string|undefined),
 *   options: (!SearchOptions|undefined),
 * }}
 */
export let SearchData;

/**
 * Files app's state.
 * @typedef {{
 *   allEntries: !Object<!FileKey, !FileData>,
 *   currentDirectory: (CurrentDirectory|undefined),
 *   search: (!SearchData|undefined),
 * }}
 */
export let State;
