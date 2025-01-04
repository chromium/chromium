// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare module globalThis {
  /* eslint-disable no-var */
  var chooseEntryResult: string|undefined;
  var step: (() => void)|null;
  var autoStep: () => void;
  var autostep: boolean;
  var currentStep: Promise<void>|null;
  /* eslint-enable no-var */
}

// TODO(b/319189127): Remove these when the integration tests extension is
// migrated to manifest v3 and can use the Promise version of these APIs.
declare namespace chrome {
  export namespace commandLinePrivate {
    export function hasSwitch(
        name: string, callback: (result: boolean) => void): void;
  }

  export namespace windows {
    export function getAll(
        queryOptions?: QueryOptions,
        callback?: (windows: Window[]) => void): void;
    export function create(
        createData?: {
          url?: string|string[],
          tabId?: number,
          left?: number,
          top?: number,
          width?: number,
          height?: number,
          focused?: boolean,
          incognito?: boolean,
          type?: CreateType,
          state?: WindowState,
          setSelfAsOpener?: boolean,
        },
        callback?: (window: Window) => void): void;
  }
  export namespace extension {
    export const inIncognitoContext: boolean;
  }
}
