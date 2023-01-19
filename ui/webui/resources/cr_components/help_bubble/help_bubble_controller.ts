// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {InsetsF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleParams} from './help_bubble.mojom-webui.js';

type Root = HTMLElement|ShadowRoot&{shadowRoot?: ShadowRoot};

export type Trackable = string|string[]|HTMLElement;

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
  private padding_: InsetsF = new InsetsF();

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

  getPadding() {
    return this.padding_;
  }

  track(trackable: Trackable, padding: InsetsF): boolean {
    assert(!this.anchor_);

    let anchor: HTMLElement|null = null;
    if (typeof trackable === 'string') {
      anchor = this.root_.querySelector<HTMLElement>(trackable);
    } else if (Array.isArray(trackable)) {
      anchor = this.deepQuery(trackable);
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
    this.padding_ = padding;
    return true;
  }

  deepQuery(selectors: string[]): HTMLElement|null {
    let cur: Root = this.root_;
    for (const selector of selectors) {
      if (cur.shadowRoot) {
        cur = cur.shadowRoot;
      }
      const el: HTMLElement|null = cur.querySelector(selector);
      if (!el) {
        return null;
      } else {
        cur = el;
      }
    }
    return cur as HTMLElement;
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
    this.bubble_.padding = this.padding_;

    if (params.timeout) {
      this.bubble_.timeoutMs = Number(params.timeout!.microseconds / 1000n);
      assert(this.bubble_.timeoutMs > 0);
    }

    assert(
        !this.bubble_.progress ||
        this.bubble_.progress.total >= this.bubble_.progress.current);

    assert(this.root_);
    this.root_.appendChild(this.bubble_);

    return this.bubble_;
  }
}
