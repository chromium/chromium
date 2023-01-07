// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.fileSystem API. */

// Declare minimal definitions for deprecated parts of the File API, which are
// not appearing in TypeScript compiler's default definiions, and are used by
// the chrome.fileSystem API.
interface FileSystemFileEntry {
  createWriter(callback: (writer: FileWriter) => void): void;
}

interface FileWriter {
  write(data: Blob): void;
}

declare namespace chrome {
  export namespace fileSystem {
    interface AcceptsOption {
      description?: string;
      mimeTypes?: string[];
      extensions?: string[];
    }

    type ChooseEntryType =
        'openFile'|'openWritableFile'|'saveFile'|'openDirectory';

    interface ChooseEntryOptions {
      type?: ChooseEntryType;
      suggestedName?: string;
      accepts?: AcceptsOption[];
      acceptsAllTypes?: boolean;
      acceptsMultiple?: boolean;
    }

    type ChoosEntryCallback = (entry?: FileSystemFileEntry) => void;

    export function chooseEntry(
        optionsOrCallback: ChooseEntryOptions,
        callback?: ChoosEntryCallback): void;
  }
}
