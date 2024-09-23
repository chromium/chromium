// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.windows API. */
// TODO(crbug.com/40179454): Auto-generate this file.

declare namespace chrome {
  export namespace windows {
    export enum WindowType {
      NORMAL = 'normal',
      POPUP = 'popup',
      PANEL = 'panel',
      APP = 'app',
      DEVTOOLS = 'devtools',
    }

    export enum WindowState {
      NORMAL = 'normal',
      MINIMIZED = 'minimized',
      MAXIMIZED = 'maximized',
      FULLSCREEN = 'fullscreen',
      LOCKED_FULLSCREEN = 'locked-fullscreen',
    }

    export interface Window {
      id?: number;
      focused: boolean;
      top?: number;
      left?: number;
      width?: number;
      height?: number;
      tabs?: chrome.tabs.Tab[];
      incognito: boolean;
      type?: WindowType;
      state?: WindowState;
      alwaysOnTop: boolean;
      sessionId?: string;
    }

    export enum CreateType {
      NORMAL = 'normal',
      POPUP = 'popup',
      PANEL = 'panel',
    }

    interface CreateData {
      url?: (string|string[]);
      tabId?: number;
      left?: number;
      top?: number;
      width?: number;
      height?: number;
      focused?: boolean;
      incognito?: boolean;
      type?: CreateType;
      state?: WindowState;
      setSelfAsOpener?: boolean;
    }

    interface QueryOptions {
      populate?: boolean;
      windowTypes?: WindowType[];
    }

    export function create(createData?: CreateData): Promise<Window>;

    export function getAll(queryOptions?: QueryOptions): Promise<Window[]>;
  }
}
