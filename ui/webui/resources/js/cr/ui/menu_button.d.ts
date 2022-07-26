// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type HideType = number;

export namespace HideType {
  export const INSTANT: number;
  export const DELAYED: number;
}

export class MenuButton extends HTMLButtonElement implements
    EventListenerObject {
  handleEvent(e: Event): void;
}
