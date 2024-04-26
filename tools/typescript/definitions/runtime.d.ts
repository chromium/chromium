// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.runtime API */
// TODO(crbug.com/40179454): Auto-generate this file.

import type {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace runtime {
      export interface Error {
        message?: string;
      }
      export let lastError: Error|undefined;

      export let id: string;

      export interface MessageSender {
        id?: string;
        tab?: chrome.tabs.Tab;
        nativeApplication?: string;
        frameId?: number;
        url?: string;
        tlsChannelId?: string;
        origin?: string;
      }

      export interface Port {
        name: string;
        disconnect: () => void;
        postMessage: (message: any) => void;
        sender?: MessageSender;
        onDisconnect: ChromeEvent<(port: Port) => void>;
        onMessage: ChromeEvent<(message: any, port: Port) => void>;
      }

      export interface ExtensionMessageEvent extends ChromeEvent<
          (message: any, sender: MessageSender,
           sendResponse: (response?: any) => void) => void> {}

      export const onMessageExternal: ExtensionMessageEvent;

      export interface PortEvent extends ChromeEvent<(port: Port) => void> {}

      export const onConnectNative: PortEvent;

      export function getURL(path: string): string;

      export function reload(): void;

      export interface SerializedContentScripts {
        matches: string[];
      }

      export interface SerializedManifest {
        manifest_version: string;
        name: string;
        version: string;
        content_scripts?: SerializedContentScripts[];
      }

      export function getManifest(): SerializedManifest;

      export function getBackgroundPage(
          callback: (backgroundPage?: Window) => void): void;

      export function sendMessage(
          extensionId: string|null, message: any, options?: {
            includeTlsChannelId?: boolean,
          },
          callback?: (response?: any) => void): void;
    }
  }
}
