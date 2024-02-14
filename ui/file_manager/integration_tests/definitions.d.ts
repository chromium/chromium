// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface Window {
  chooseEntryResult?: Entry|null;
  step: (() => void)|null;
  autoStep(): void;
  autostep: boolean;
  currentStep: Promise<void>|null;
}


// Copied from the fileManagerPrivate.
// TODO(TS): Remove this interface when file_manager_private.d.ts is
// auto-generated and submitted.
interface FileTaskDescriptor {
  appId: string;
  taskType: string;
  actionId: string;
}


// TODO(b/319189127): Remove this when the integration tests extension is
// migrated to manifest v3 and can use the Promise version of this API.
declare namespace chrome {
  export namespace commandLinePrivate {
    export function hasSwitch(
        name: string, callback: (result: boolean) => void): void;
  }
}
