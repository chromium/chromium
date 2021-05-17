// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface FindShortcutBehaviorInterface {
  findShortcutListenOnAttach: boolean;
  becomeActiveFindShortcutListener(): void;
  handleFindShortcut(modalContextOpen: boolean): boolean;
  removeSelfAsFindShortcutListener(): void;
  searchInputHasFocus(): boolean;
}

export {FindShortcutBehavior};

interface FindShortcutBehavior extends FindShortcutBehaviorInterface {}

declare const FindShortcutBehavior: object;
