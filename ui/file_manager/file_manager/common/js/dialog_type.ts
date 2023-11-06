// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

export function isModal(type: DialogType): boolean {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER ||
      type == DialogType.SELECT_SAVEAS_FILE ||
      type == DialogType.SELECT_OPEN_FILE ||
      type == DialogType.SELECT_OPEN_MULTI_FILE;
}

export function isFolderDialogType(type: DialogType): boolean {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER;
}
