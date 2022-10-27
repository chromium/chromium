// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MultiMenuButton extends HTMLButtonElement {
  showMenu(shouldFocus: boolean, mousePos?: {x: number, y: number}): void;
}
