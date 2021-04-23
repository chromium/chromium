// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';
import {CrContainerShadowBehavior} from '../cr_container_shadow_behavior.m.js';

interface CrDialogElement extends CrContainerShadowBehavior, LegacyElementMixin,
                                  HTMLElement {
  open: boolean|null|undefined;
  closeText: string|null|undefined;
  ignorePopstate: boolean|null|undefined;
  ignoreEnterKey: boolean|null|undefined;
  consumeKeydownEvent: boolean|null|undefined;
  noCancel: boolean|null|undefined;
  showCloseButton: boolean|null|undefined;
  showOnAttach: boolean|null|undefined;
  showModal(): void;
  cancel(): void;
  close(): void;
  setTitleAriaLabel(title: string): void;
  getNative(): HTMLDialogElement;
  focus(): void;
}

export {CrDialogElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-dialog': CrDialogElement;
  }
}
