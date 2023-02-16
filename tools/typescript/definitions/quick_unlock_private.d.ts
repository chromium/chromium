// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.quickUnlockPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace quickUnlockPrivate {
      export interface TokenInfo {
        token: string;
        lifetimeSeconds: number;
      }

      export enum QuickUnlockMode {
        PIN = 'PIN',
      }

      // TODO(crbug/1368302) Update to use promises instead of callback
      export const onActiveModesChanged:
          ChromeEvent<(activeModes: QuickUnlockMode[]) => void>;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function canAuthenticatePin(
          onComplete: (success: boolean) => void): void;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function getActiveModes(
          onComplete: (modes: QuickUnlockMode[]) => void): void;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function getAuthToken(
          accountPassword: string, onComplete: (info: TokenInfo) => void): void;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function setLockScreenEnabled(
          token: string, enabled: boolean, onComplete?: () => void): void;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function setModes(
          token: string, modes: QuickUnlockMode[], credentials: string[],
          onComplete: () => void): void;

      // TODO(crbug/1368302) Update to use promises instead of callback
      export function setPinAutosubmitEnabled(
          token: string, pin: string, enabled: boolean,
          onComplete: (success: boolean) => void): void;
    }
  }
}
