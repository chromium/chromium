// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.windows API. */
// TODO(crbug.com/1203307): Auto-generate this file.

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

    export interface GetInfo {
      populate?: boolean;
      windowTypes?: WindowType[];
    }

    export const WINDOW_ID_NONE: number;
    export const WINDOW_ID_CURRENT: number;

    export function get(windowId: number, getInfo: (GetInfo|null),
                        callback: (p1: Window) => void): void;
    export function getCurrent(getInfo: (GetInfo|null),
                               callback: (p1: Window) => void): void;
    export function getLastFocused(getInfo: (GetInfo|null),
                                   callback: (p1: Window) => void): void;
    export function getAll(getInfo: (GetInfo|null),
                           callback: (p1: Window[]) => void): void;
    type CreateData = {
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

    export function create(createData?: CreateData,
                           callback?: (p1: Window) => void): void;

    type UpdateInfo = {
      left?: number;
      top?: number;
      width?: number;
      height?: number;
      focused?: boolean;
      drawAttention?: boolean;
      state?: WindowState;
    }

    export function update(windowId: number, updateInfo: UpdateInfo,
                           callback?: (p1: Window) => void): void;
    export function remove(windowId: number, callback?: () => void): void;
  }
}
