// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {InsetsF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import type {HelpBubbleElement} from './help_bubble.js';
import {HELP_BUBBLE_SCROLL_ANCHOR_OPTIONS} from './help_bubble.js';
import type {HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {HelpBubbleArrowPosition} from './help_bubble.mojom-webui.js';

type Root = HTMLElement|ShadowRoot&{shadowRoot?: ShadowRoot};

export type Trackable = string|string[]|HTMLElement|Element;

export const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

interface Options {
  padding: InsetsF;
  fixed: boolean;
}

// Return whether the current language is right-to-left
function isRtlLang(element: HTMLElement) {
  return window.getComputedStyle(element).direction === 'rtl';
}

// Reflect arrow position across y-axis
function reflectArrowPosition(position: HelpBubbleArrowPosition) {
  switch (position) {
    case HelpBubbleArrowPosition.TOP_LEFT:
      return HelpBubbleArrowPosition.TOP_RIGHT;

    case HelpBubbleArrowPosition.TOP_RIGHT:
      return HelpBubbleArrowPosition.TOP_LEFT;

    case HelpBubbleArrowPosition.BOTTOM_LEFT:
      return HelpBubbleArrowPosition.BOTTOM_RIGHT;

    case HelpBubbleArrowPosition.BOTTOM_RIGHT:
      return HelpBubbleArrowPosition.BOTTOM_LEFT;

    case HelpBubbleArrowPosition.LEFT_TOP:
      return HelpBubbleArrowPosition.RIGHT_TOP;

    case HelpBubbleArrowPosition.LEFT_CENTER:
      return HelpBubbleArrowPosition.RIGHT_CENTER;

    case HelpBubbleArrowPosition.LEFT_BOTTOM:
      return HelpBubbleArrowPosition.RIGHT_BOTTOM;

    case HelpBubbleArrowPosition.RIGHT_TOP:
      return HelpBubbleArrowPosition.LEFT_TOP;

    case HelpBubbleArrowPosition.RIGHT_CENTER:
      return HelpBubbleArrowPosition.LEFT_CENTER;

    case HelpBubbleArrowPosition.RIGHT_BOTTOM:
      return HelpBubbleArrowPosition.LEFT_BOTTOM;

    default:
      return position;
  }
}

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
  private bubble_: HelpBubbleElement|null = null;
  private options_:
      Options = {padding: {top: 0, bottom: 0, left: 0, right: 0}, fixed: false};

  /**
   * Whether a help bubble (webui or external) is being shown for this
   * controller
   */
  private isBubbleShowing_: boolean = false;

  /** Keep track of last known anchor visibility status. */
  private isAnchorVisible_: boolean = false;

  /** Keep track of last known anchor bounds. */
  private lastAnchorBounds_: RectF = {x: 0, y: 0, width: 0, height: 0};

  /*
   * This flag is used to know whether to send position updates for
   * external bubbles
   */
  private isExternal_: boolean = false;

  constructor(nativeId: string, root: ShadowRoot) {
    assert(
        nativeId,
        'HelpBubble: nativeId was not defined when registering help bubble');
    assert(
        root,
        'HelpBubble: shadowRoot was not defined when registering help bubble');

    this.nativeId_ = nativeId;
    this.root_ = root;
  }

  isBubbleShowing() {
    return this.isBubbleShowing_;
  }

  canShowBubble() {
    return this.hasAnchor();
  }

  hasBubble() {
    return !!this.bubble_;
  }

  getBubble() {
    return this.bubble_;
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
    return this.options_.padding;
  }

  getAnchorVisibility() {
    return this.isAnchorVisible_;
  }

  getLastAnchorBounds() {
    return this.lastAnchorBounds_;
  }

  updateAnchorVisibility(isVisible: boolean, bounds: RectF): boolean {
    const changed = isVisible !== this.isAnchorVisible_ ||
        bounds.x !== this.lastAnchorBounds_.x ||
        bounds.y !== this.lastAnchorBounds_.y ||
        bounds.width !== this.lastAnchorBounds_.width ||
        bounds.height !== this.lastAnchorBounds_.height;
    this.isAnchorVisible_ = isVisible;
    this.lastAnchorBounds_ = bounds;
    return changed;
  }

  isAnchorFixed(): boolean {
    return this.options_.fixed;
  }

  isExternal() {
    return this.isExternal_;
  }

  updateExternalShowingStatus(isShowing: boolean) {
    this.isExternal_ = true;
    this.isBubbleShowing_ = isShowing;
    this.setAnchorHighlight_(isShowing);
  }

  track(trackable: Trackable, options: Options): boolean {
    assert(!this.anchor_);

    let anchor: HTMLElement|null = null;
    if (typeof trackable === 'string') {
      anchor = this.root_.querySelector<HTMLElement>(trackable);
    } else if (Array.isArray(trackable)) {
      anchor = this.deepQuery(trackable);
    } else if (trackable instanceof HTMLElement) {
      anchor = trackable;
    } else {
      assertNotReached(
          'HelpBubble: anchor argument was unrecognized when registering ' +
          'help bubble');
    }

    if (!anchor) {
      return false;
    }

    anchor.dataset['nativeId'] = this.nativeId_;
    this.anchor_ = anchor;
    this.options_ = options;
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

  show() {
    this.isExternal_ = false;
    if (!(this.bubble_ && this.anchor_)) {
      return;
    }
    this.bubble_.show(this.anchor_);
    this.isBubbleShowing_ = true;
    this.setAnchorHighlight_(true);
  }

  hide() {
    if (!this.bubble_) {
      return;
    }
    this.bubble_.hide();
    this.bubble_.remove();
    this.bubble_ = null;
    this.isBubbleShowing_ = false;
    this.setAnchorHighlight_(false);
  }

  createBubble(params: HelpBubbleParams): HelpBubbleElement {
    assert(
        this.anchor_,
        'HelpBubble: anchor was not defined when showing help bubble');
    assert(this.anchor_.parentNode, 'HelpBubble: anchor element not in DOM');

    this.bubble_ = document.createElement('help-bubble');
    this.bubble_.nativeId = this.nativeId_;
    this.bubble_.position = isRtlLang(this.anchor_) ?
        reflectArrowPosition(params.position) :
        params.position;
    this.bubble_.closeButtonAltText = params.closeButtonAltText;
    this.bubble_.bodyText = params.bodyText;
    this.bubble_.bodyIconName = params.bodyIconName || null;
    this.bubble_.bodyIconAltText = params.bodyIconAltText;
    this.bubble_.titleText = params.titleText || '';
    this.bubble_.progress = params.progress || null;
    this.bubble_.buttons = params.buttons;
    this.bubble_.padding = this.options_.padding;
    this.bubble_.focusAnchor = params.focusOnShowHint === false;

    if (params.timeout) {
      this.bubble_.timeoutMs = Number(params.timeout!.microseconds / 1000n);
      assert(this.bubble_.timeoutMs > 0);
    }

    assert(
        !this.bubble_.progress ||
        this.bubble_.progress.total >= this.bubble_.progress.current);

    assert(this.root_);

    // Because the help bubble uses either absolute or fixed positioning, it
    // need only be placed within the offset parent of the anchor. However it is
    // placed as a sibling to the anchor because that guarantees proper tab
    // order.
    if (getComputedStyle(this.anchor_).getPropertyValue('position') ===
        'fixed') {
      this.bubble_.fixed = true;
    }
    this.anchor_.parentNode.insertBefore(this.bubble_, this.anchor_);
    return this.bubble_;
  }

  /**
   * Styles the anchor element to appear highlighted while the bubble is open,
   * or removes the highlight.
   */
  private setAnchorHighlight_(highlight: boolean) {
    assert(
        this.anchor_, 'Set anchor highlight: expected valid anchor element.');
    this.anchor_.classList.toggle(ANCHOR_HIGHLIGHT_CLASS, highlight);
    if (highlight) {
      (this.bubble_ || this.anchor_).focus();
      this.anchor_.scrollIntoView(HELP_BUBBLE_SCROLL_ANCHOR_OPTIONS);
    }
  }
}
