// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The types and enums in this file are used in integration tests.
 * For this reason we don't want additional imports in here to avoid cascading
 * importing files.
 */

/** Paths that can be handled by the dialog opener in native code. */
export enum AllowedPaths {
  NATIVE_PATH = 'nativePath',
  ANY_PATH = 'anyPath',
  ANY_PATH_OR_URL = 'anyPathOrUrl',
}

/** The type of each volume. */
export enum VolumeType {
  TESTING = 'testing',
  DRIVE = 'drive',
  DOWNLOADS = 'downloads',
  REMOVABLE = 'removable',
  ARCHIVE = 'archive',
  MTP = 'mtp',
  PROVIDED = 'provided',
  MEDIA_VIEW = 'media_view',
  DOCUMENTS_PROVIDER = 'documents_provider',
  CROSTINI = 'crostini',
  GUEST_OS = 'guest_os',
  ANDROID_FILES = 'android_files',
  MY_FILES = 'my_files',
  SMB = 'smb',
  SYSTEM_INTERNAL = 'system_internal',
  TRASH = 'trash',
}

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 */
export enum DialogType {
  SELECT_FOLDER = 'folder',
  SELECT_UPLOAD_FOLDER = 'upload-folder',
  SELECT_SAVEAS_FILE = 'saveas-file',
  SELECT_OPEN_FILE = 'open-file',
  SELECT_OPEN_MULTI_FILE = 'open-multi-file',
  FULL_PAGE = 'full-page',
}



/**
 * A list of extensions with a corresponding description for them, e.g.
 * { extensions: ['htm', 'html'], description: 'HTML' }
 */
export interface TypeList {
  extensions: string[];
  description: string;
  selected: boolean;
}

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
 */
export class FilesAppState {
  /**
   * The desired target directory when opening a new window.
   */
  currentDirectoryURL?: string|null;

  /**
   * The URL for a file or directory to be selected once a new window is
   * spawned.
   */
  selectionURL?: string;

  /**
   * For SaveAs dialog it prefills the <input> for the file name with this
   * value.
   * For FilePicker it pre-selects the file in the file list.
   */
  targetName?: string;

  /**
   * Search term to initialize the Files app directly in a search results.
   */
  searchQuery?: string;

  /**
   * The type of the window being opened, when it's undefined it defaults to
   * the normal Files app window (non-dialog version).
   */
  type?: DialogType;

  /**
   * List of file extensions (.txt, .zip, etc) that will be used by
   * AndroidAppListModel, when displaying Files app as FilePicker for ARC++.
   * Files app displays Android apps that can handle such extensions in the
   * DirectoryTree.
   */
  typeList?: TypeList[];

  /**
   * For FilePicker indicates that the "All files" should be displayed in the
   * file type dropdown in the footer.
   */
  includeAllFiles?: boolean;

  /**
   * Defines what volumes are available in the Files app, when NATIVE_PATH is
   * used, any virtual volume (FSPs) is hidden.
   *
   * Defaults to `ANY_PATH_OR_URL` when undefined.
   */
  allowedPaths?: AllowedPaths;

  /**
   * If the Android apps should be shown in the DirectoryTree for FilePicker.
   */
  showAndroidPickerApps?: boolean;

  /**
   * Array of Files app mode dependent volume filter names. Defaults to an
   * empty Array when undefined, and is the normal case (no filters).
   *
   * See filtered_volume_manager.js for details about the available volume
   * filter names and their volume filter effects.
   */
  volumeFilter?: string[];
}

/**
 * Stats collected about Metadata handling for tests.
 */
export class MetadataStats {
  /** Total of entries fulfilled from cache. */
  fromCache: number = 0;

  /** Total of entries that requested to backends. */
  fullFetch: number = 0;

  /** Total of entries that called to invalidate. */
  invalidateCount: number = 0;

  /** Total of entries that called to clear. */
  clearCacheCount: number = 0;

  /** Total of calls to function clearAllCache. */
  clearAllCount: number = 0;
}

export interface ElementObject {
  attributes: Record<string, string|null>;
  text: string|null;
  innerText: string|null;
  value: string|null;
  styles?: Record<string, string>;
  hidden: boolean;
  hasShadowRoot: boolean;
  imageWidth?: number;
  imageHeight?: number;
  renderedWidth?: number;
  renderedHeight?: number;
  renderedTop?: number;
  renderedLeft?: number;
  scrollLeft?: number;
  scrollTop?: number;
  scrollWidth?: number;
  scrollHeight?: number;
}

/**
 * Object containing common key modifiers: shift, alt, and ctrl.
 */
export interface KeyModifiers {
  shift?: boolean;
  alt?: boolean;
  ctrl?: boolean;
}
