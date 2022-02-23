// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';
import {CrContainerShadowBehavior} from '../cr_container_shadow_behavior.m.js';

interface CrDialogElement extends CrContainerShadowBehavior, LegacyElementMixin,
                                  HTMLElement {
  open: boolean;
  closeText: string|null|undefined;
  ignorePopstate: boolean;
  ignoreEnterKey: boolean;
  consumeKeydownEvent: boolean;
  noCancel: boolean;
  showCloseButton: boolean;
  showOnAttach: boolean;
  showModal(): void;
  cancel(): void;
  close(): void;
  setTitleAriaLabel(title: string): void;
  getNative(): HTMLDialogElement;
  focus(): void;

  $: {
    close: HTMLElement,
  };
}

export {CrDialogElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-dialog': CrDialogElement;
  }
}
