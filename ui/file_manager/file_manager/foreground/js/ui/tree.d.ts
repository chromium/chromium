// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';

export class Tree extends HTMLElement {
  items(): TreeItem[];
}

export class TreeItem extends HTMLElement {
  get parentItem(): TreeItem|Tree|undefined;
  get entry(): Entry|FilesAppEntry|DirectoryEntry|null|undefined;
}
