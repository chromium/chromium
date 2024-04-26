// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.inputMethodPrivate API */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace inputMethodPrivate {
      export enum LanguagePackStatus {
        UNKNOWN = 'unknown',
        NOT_INSTALLED = 'notInstalled',
        IN_PROGRESS = 'inProgress',
        INSTALLED = 'installed',
        ERROR_OTHER = 'errorOther',
        ERROR_NEEDS_REBOOT = 'errorNeedsReboot',
      }

      export interface LanguagePackStatusChange {
        engineIds: string[];
        status: LanguagePackStatus;
      }

      export function getCurrentInputMethod(): Promise<string>;
      export function setCurrentInputMethod(inputMethodId: string):
          Promise<void>;

      export function openOptionsPage(id: string): void;

      export function getLanguagePackStatus(inputMethodId: string):
          Promise<LanguagePackStatus>;

      export const onChanged: ChromeEvent<(newInputMethodId: string) => void>;
      export const onLanguagePackStatusChanged:
          ChromeEvent<(change: LanguagePackStatusChange) => void>;
    }
  }
}
