// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../files_app_entry_interfaces.js';

/**
 * The data for each individual file/entry.
 * @typedef {{
 *   entry: (Entry|FilesAppEntry),
 *   label: string,
 *   volumeType: (VolumeManagerCommon.VolumeType|null),
 * }}
 */
export let FileData;

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 * @typedef {{
 *   name: string,
 *   label: string,
 *   key: string,
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
 * The additional property states understood by the search container. When the
 * user actions cause search elements to be hidden, the search state in the
 * store becomes INACTIVE.
 * @enum {string}
 */
export const SearchStatus = {
  INACTIVE: 'INACTIVE',
};

/**
 * The current directory.
 * The directory is only effectively active when the `status` is SUCCESS.
 * @typedef {{
 *   status: !PropStatus,
 *   key: string,
 *   pathComponents: !Array<PathComponent>,
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
 *   status: (PropStatus|SearchStatus|undefined),
 *   query: (string|undefined),
 *   options: (!SearchOptions|undefined),
 * }}
 */
export let SearchData;

/**
 * Files app's state.
 * @typedef {{
 *   allEntries: !Object<string, FileData>,
 *   currentDirectory: (CurrentDirectory|undefined),
 *   search: (!SearchData|undefined),
 * }}
 */
export let State;
