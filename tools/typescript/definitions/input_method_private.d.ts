// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.inputMethodPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace inputMethodPrivate {
      export function getCurrentInputMethod(): Promise<string>;
      export function setCurrentInputMethod(inputMethodId: string):
          Promise<void>;

      export function openOptionsPage(id: string): void;

      export const onChanged: ChromeEvent<(newInputMethodId: string) => void>;
    }
  }
}
