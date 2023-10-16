// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from './dialog_type.js';
import {AllowedPaths} from './volume_manager_types.js';

/**
 * FilesAppState is used in 2 ways:
 *
 * 1. Persist in the localStorage the some state, like current directory,
 * sorting column options, etc.
 *
 * 2. To open a new window:
 * 2.1, Requests to open a new window set part of these options to configure the
 * how the new window should behave.
 * 2.2  When the Files app extension is restarted, the background page retrieves
 * the last state from localStorage for each opened window and re-spawn the
 * windows with their state.
 *
 * @record
 */
export class FilesAppState {
  constructor() {
    /**
     * The desired target directory when opening a new window.
     * @public @type {string|null|undefined}
     */
    this.currentDirectoryURL = undefined;

    /**
     * The URL for a file or directory to be selected once a new window is
     * spawned.
     * @public @type {string|undefined}
     */
    this.selectionURL = undefined;

    /**
     * For SaveAs dialog it prefills the <input> for the file name with this
     * value.
     * For FilePicker it pre-selects the file in the file list.
     * @public @type {string|undefined}
     */
    this.targetName;

    /**
     * Search term to initialize the Files app directly in a search results.
     * @public @type {string|undefined}
     */
    this.searchQuery;

    /**
     * The type of the window being opened, when it's undefined it defaults to
     * the normal Files app window (non-dialog version).
     * @public @type {!DialogType|undefined}
     */
    this.type;

    /**
     * List of file extensions (.txt, .zip, etc) that will be used by
     * AndroidAppListModel, when displaying Files app as FilePicker for ARC++.
     * Files app displays Android apps that can handle such extensions in the
     * DirectoryTree.
     * TODO(lucmult): Add type for the Object below:
     * @public @type {!Array<!Object>|undefined}
     */
    this.typeList;

    /**
     * For FilePicker indicates that the "All files" should be displayed in the
     * file type dropdown in the footer.
     * @public @type {boolean|undefined}
     */
    this.includeAllFiles;

    /**
     * Defines what volumes are available in the Files app, when NATIVE_PATH is
     * used, any virtual volume (FSPs) is hidden.
     *
     * Defaults to `ANY_PATH_OR_URL` when undefined.
     * @public @type {!AllowedPaths|undefined}
     */
    this.allowedPaths;

    /**
     * If the Android apps should be shown in the DirectoryTree for FilePicker.
     * @public @type {boolean|undefined}
     */
    this.showAndroidPickerApps;

    /**
     * Array of Files app mode dependent volume filter names. Defaults to an
     * empty Array when undefined, and is the normal case (no filters).
     *
     * See filtered_volume_manager.js for details about the available volume
     * filter names and their volume filter effects.
     *
     * @public @type {!Array<string>|undefined}
     */
    this.volumeFilter;
  }
}
