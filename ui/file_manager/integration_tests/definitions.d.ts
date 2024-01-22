// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface Window {
  chooseEntryResult?: Entry|null;
}


// Copied from the fileManagerPrivate.
// TODO(TS): Remove this interface when file_manager_private.d.ts is
// auto-generated and submitted.
interface FileTaskDescriptor {
  appId: string;
  taskType: string;
  actionId: string;
}
