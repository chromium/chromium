// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.contextMenus API
 * Partially generated from: chrome/common/extensions/api/context_menus.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/context_menus.json -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace contextMenus {

      export const ACTION_MENU_TOP_LEVEL_LIMIT: number;

      export enum ContextType {
        ALL = 'all',
        PAGE = 'page',
        FRAME = 'frame',
        SELECTION = 'selection',
        LINK = 'link',
        EDITABLE = 'editable',
        IMAGE = 'image',
        VIDEO = 'video',
        AUDIO = 'audio',
        LAUNCHER = 'launcher',
        BROWSER_ACTION = 'browser_action',
        PAGE_ACTION = 'page_action',
        ACTION = 'action',
      }

      export enum ItemType {
        NORMAL = 'normal',
        CHECKBOX = 'checkbox',
        RADIO = 'radio',
        SEPARATOR = 'separator',
      }

      export interface OnClickData {
        menuItemId: number|string;
        parentMenuItemId?: number|string;
        mediaType?: string;
        linkUrl?: string;
        srcUrl?: string;
        pageUrl?: string;
        frameUrl?: string;
        frameId?: number;
        selectionText?: string;
        editable: boolean;
        wasChecked?: boolean;
        checked?: boolean;
      }

      export interface CreateProperties {
        type?: ItemType;
        id?: string;
        title?: string;
        checked?: boolean;
        contexts?: ContextType[];
        visible?: boolean;
        onclick?: (info: OnClickData, tab: tabs.Tab) => void;
        parentId?: number|string;
        documentUrlPatterns?: string[];
        targetUrlPatterns?: string[];
        enabled?: boolean;
      }

      export function create(createProperties: CreateProperties): number|string;

      export function update(id: number|string, updateProperties: {
        type?: ItemType,
        title?: string,
        checked?: boolean,
        contexts?: ContextType[],
        visible?: boolean,
        onclick?: (info: OnClickData, tab: tabs.Tab) => void,
        parentId?: number|string,
        documentUrlPatterns?: string[],
        targetUrlPatterns?: string[],
        enabled?: boolean,
      }): Promise<void>;

      export function remove(menuItemId: number|string): Promise<void>;

      export function removeAll(): Promise<void>;

      export const onClicked:
          ChromeEvent<(info: OnClickData, tab?: tabs.Tab) => void>;

    }
  }
}
