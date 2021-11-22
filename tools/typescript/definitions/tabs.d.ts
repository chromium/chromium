// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.tabs API. */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace tabs {
    export enum TabStatus {
      UNLOADED = 'unloaded',
      LOADING = 'loading',
      COMPLETE = 'complete',
    }

    export enum MutedInfoReason {
      USER = 'user',
      CAPTURE = 'capture',
      EXTENSION = 'extension',
    }

    export interface MutedInfo {
      muted: boolean;
      reason?: MutedInfoReason;
      extensionId?: string;
    }

    export interface Tab {
      id?: number;
      index: number;
      groupId: number;
      windowId: number;
      openerTabId?: number;
      selected: boolean;
      highlighted: boolean;
      active: boolean;
      pinned: boolean;
      audible?: boolean;
      discareded: boolean;
      autoDiscardable: boolean;
      mutedInfo?: MutedInfo;
      url?: string;
      pendingUrl?: string;
      title?: string;
      favIconUrl?: string;
      status?: TabStatus;
      incognito: boolean;
      width?: number;
      height?: number;
      sessionId?: string;
    }

    type CreateProperties = {
      windowId?: number;
      index?: number;
      url?: string;
      active?: boolean;
      selected?: boolean;
      pinned?: boolean;
      openerTabId?: number;
    }

    type UpdateProperties = {
      url?: string,
      active?: boolean,
      highlighted?: boolean,
      selected?: boolean,
      pinned?: boolean,
      muted?: boolean,
      openerTabId?: number,
      autoDiscardable?: boolean,
    }

    export function
    create(createProperties: CreateProperties, callback?: (p1: Tab) => void):
        void;

    export function update(
        tabId: number|undefined, updateProperties: UpdateProperties,
        callback?: (p1?: Tab) => void): void;
  }
}
