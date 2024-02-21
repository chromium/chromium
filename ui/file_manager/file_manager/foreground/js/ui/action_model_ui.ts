// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAlertDialog} from './files_alert_dialog.js';
import type {ListContainer} from './list_container.js';

export interface ActionModelUi {
  alertDialog: FilesAlertDialog;
  listContainer: ListContainer;
}
