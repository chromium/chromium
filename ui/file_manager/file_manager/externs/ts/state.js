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
 * Data for search. It should be empty `{}` when the user isn't searching.
 * @typedef {{
 *   status: (PropStatus|undefined),
 *   query: (string|undefined),
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
