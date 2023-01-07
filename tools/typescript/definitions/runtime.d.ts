// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.runtime API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace runtime {
      export let lastError: {
        message?: string,
      } | undefined;

      export interface MessageSender {
        id?: string;
        tab?: chrome.tabs.Tab;
        nativeApplication?: string;
        frameId?: number;
        url?: string;
        tlsChannelId?: string;
        origin?: string;
      }

      export interface ExtensionMessageEvent
        extends ChromeEvent<(
          message: any,
          sender: MessageSender,
          sendResponse: (response?: any) => void) => void> { }
      export const onMessageExternal: ExtensionMessageEvent;
    }
  }
}
