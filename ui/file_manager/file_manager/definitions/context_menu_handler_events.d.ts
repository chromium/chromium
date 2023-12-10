// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Menu} from '../foreground/js/ui/menu.js';

interface ContextMenuEventDetail {
  element: HTMLElement;
  menu: Menu;
}

export type HideEvent = CustomEvent<ContextMenuEventDetail>;

export type ShowEvent = CustomEvent<ContextMenuEventDetail>;
