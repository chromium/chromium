// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../../state/state.js';

export function isModal(type: DialogType): boolean {
  return type === DialogType.SELECT_FOLDER ||
      type === DialogType.SELECT_UPLOAD_FOLDER ||
      type === DialogType.SELECT_SAVEAS_FILE ||
      type === DialogType.SELECT_OPEN_FILE ||
      type === DialogType.SELECT_OPEN_MULTI_FILE;
}

export function isFolderDialogType(type: DialogType): boolean {
  return type === DialogType.SELECT_FOLDER ||
      type === DialogType.SELECT_UPLOAD_FOLDER;
}
