// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.resourcesPrivate API. */
// TODO(crbug.com/40179454): Auto-generate this file.

declare namespace chrome {
  export namespace resourcesPrivate {
    export enum Component {
      IDENTITY = 'identity',
      PDF = 'pdf',
    }

    export function getStrings(
        component: Component,
        callback: (strings: {[key: string]: string}) => void): void;
  }
}
