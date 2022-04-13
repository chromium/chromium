// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Menu} from './menu.m.js';
import {HideType} from './menu_button.m.js';

export const contextMenuHandler: ContextMenuHandler;

declare class ContextMenuHandler extends EventTarget implements
    EventListenerObject {
  get menu(): Menu;
  showMenu(e: Event, menu: Menu): void;
  hideMenu(hideType?: HideType|undefined): void;
  handleEvent(e: Event): void;
  addContextMenuProperty(elementOrClass: Element|Function): void;
  setContextMenu(element: Element, contextMenu: Menu): void;
}
