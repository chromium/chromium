// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.tabs API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
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

      interface CreateProperties {
        windowId?: number;
        index?: number;
        url?: string;
        active?: boolean;
        selected?: boolean;
        pinned?: boolean;
        openerTabId?: number;
      }

      interface UpdateProperties {
        url?: string;
        active?: boolean;
        highlighted?: boolean;
        selected?: boolean;
        pinned?: boolean;
        muted?: boolean;
        openerTabId?: number;
        autoDiscardable?: boolean;
      }

      enum ZoomSettingsMode {
        AUTOMATIC = 'automatic',
        MANUAL = 'manual',
        DISABLED = 'disabled',
      }

      enum ZoomSettingsScope {
        PER_ORIGIN = 'per-origin',
        PER_TAB = 'per-tab',
      }

      export interface ZoomSettings {
        mode?: ZoomSettingsMode;
        scope?: ZoomSettingsScope;
        defaultZoomFactor?: number;
      }

      export const TAB_ID_NONE: number;

      export function get(tabId: number, callback: (tab: Tab) => void): void;

      export function getCurrent(callback: (tab?: Tab) => void): void;

      export function create(
          createProperties: CreateProperties,
          callback?: (p1: Tab) => void): void;

      export function update(
          tabId: number|undefined, updateProperties: UpdateProperties,
          callback?: (p1?: Tab) => void): void;

      export function reload(tabId: number): void;

      export function query(queryInfo: QueryInfo): Promise<Tab[]>;

      export function setZoom(
          tabId: number|undefined, zoomFactor: number,
          callback?: () => void): void;
      export function getZoom(
          tabId: number|undefined, callback: (zoom: number) => void): void;
      export function setZoomSettings(
          tabId: number|undefined, zoomSettings: ZoomSettings,
          callback?: () => void): void;
      export function getZoomSettings(
          tabId: number|undefined,
          callback: (settings: ZoomSettings) => void): void;

      interface ActiveInfo {
        tabId: number;
        windowId: number;
      }

      interface ChangeInfo {}

      interface QueryInfo {
        active?: boolean;
        currentWindow?: boolean;
      }

      interface ZoomChangeInfo {
        tabId: number;
        newZoomFactor: number;
      }

      export const onActivated: ChromeEvent<(info: ActiveInfo) => void>;
      export const onUpdated: ChromeEvent<(
        tabId: number, changeInfo: ChangeInfo, tab: Tab) => void>;
      export const onZoomChange: ChromeEvent<(info: ZoomChangeInfo) => void>;
    }
  }
}
