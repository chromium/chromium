// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.indigoPrivate API. */

export {};

declare global {
  export namespace chrome {
    export namespace indigoPrivate {
      export interface OnRegenerateStartedEvent {
        addListener(
            listener: () => void,
            filter?: {instanceId: number}): void;
        removeListener(listener: () => void): void;
        hasListener(listener: () => void): boolean;
      }

      export interface ImageData {
        value: ArrayBuffer | string;
      }
      export function readyToRender(): Promise<number>;
      export function getOriginalImage(): Promise<ImageData>;
      export function getReplacementImage(): Promise<ImageData>;
      export const onRegenerateStarted: OnRegenerateStartedEvent;
    }
  }
}
