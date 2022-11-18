// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleParams} from './help_bubble.mojom-webui.js';

export type Trackable = string|HTMLElement;

/**
 * HelpBubble controller class
 * - There should exist only one HelpBubble instance for each nativeId
 * - The mapping between nativeId and htmlId is held within this instance
 * - The rest of the parameters are passed to createBubble
 */
export class HelpBubbleController {
  private nativeId_: string;
  private root_: ShadowRoot;
  private anchor_: HTMLElement|null = null;
  private showing_: boolean = false;
  private bubble_: HelpBubbleElement|null = null;

  constructor(nativeId: string, root: ShadowRoot) {
    assert(nativeId && root);

    this.nativeId_ = nativeId;
    this.root_ = root;
  }

  isShowing() {
    return this.showing_;
  }

  canShow() {
    return this.hasAnchor();
  }

  hasAnchor() {
    return !!this.anchor_;
  }

  getAnchor() {
    return this.anchor_;
  }

  getNativeId() {
    return this.nativeId_;
  }

  track(trackable: Trackable): boolean {
    assert(!this.anchor_);

    let anchor: HTMLElement|null = null;
    if (typeof trackable === 'string') {
      anchor = this.root_.querySelector<HTMLElement>(trackable);
    } else if (trackable instanceof HTMLElement) {
      anchor = trackable;
    } else {
      assertNotReached('HelpBubbleController.track() - anchor is unrecognized');
    }

    if (!anchor) {
      return false;
    }

    anchor.dataset['nativeId'] = this.nativeId_;
    this.anchor_ = anchor;
    return true;
  }

  hasElement() {
    return !!this.bubble_;
  }

  getElement() {
    return this.bubble_;
  }

  show() {
    if (!(this.bubble_ && this.anchor_)) {
      return;
    }
    this.bubble_.show(this.anchor_);
    this.anchor_.focus();
    this.showing_ = true;
  }

  hide() {
    if (!this.bubble_) {
      return;
    }
    this.bubble_.hide();
    this.bubble_.remove();
    this.bubble_ = null;
    this.showing_ = false;
  }

  createBubble(params: HelpBubbleParams): HelpBubbleElement {
    assert(this.anchor_);

    this.bubble_ = document.createElement('help-bubble');
    this.bubble_.nativeId = this.nativeId_;
    this.bubble_.position = params.position;
    this.bubble_.closeButtonAltText = params.closeButtonAltText;
    this.bubble_.bodyText = params.bodyText;
    this.bubble_.bodyIconName = params.bodyIconName || null;
    this.bubble_.bodyIconAltText = params.bodyIconAltText;
    this.bubble_.forceCloseButton = params.forceCloseButton;
    this.bubble_.titleText = params.titleText || '';
    this.bubble_.progress = params.progress || null;
    this.bubble_.buttons = params.buttons;

    if (params.timeout) {
      this.bubble_.timeoutMs = Number(params.timeout!.microseconds / 1000n);
      assert(this.bubble_.timeoutMs > 0);
    }

    assert(
        !this.bubble_.progress ||
        this.bubble_.progress.total >= this.bubble_.progress.current);

    // insert after anchor - if nextSibling is null, bubble will
    // be added as the last child of parentNode
    this.anchor_!.parentNode!.insertBefore(
        this.bubble_, this.anchor_!.nextSibling);

    return this.bubble_;
  }
}
