// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {InsetsF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleParams} from './help_bubble.mojom-webui.js';

type Root = HTMLElement|ShadowRoot&{shadowRoot?: ShadowRoot};

export type Trackable = string|string[]|HTMLElement|Element;

export const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

interface Options {
  padding: InsetsF;
  fixed: boolean;
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
  private options_: Options = {padding: new InsetsF(), fixed: false};

  /**
   * Whether the anchor element is contained in an element that is scrollable
   * but is not the document body. These elements require different visibility
   * handling as they will not technically intersect the document body when
   * they're scrolled out of view.
   */
  private isNonBodyScrollable_: boolean = false;

  /**
   * Whether a help bubble (webui or external) is being shown for this
   * controller
   */
  private isBubbleShowing_: boolean = false;

  // Keep track of last-known anchor visibility status
  private isAnchorVisible_: boolean = false;

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

  cacheAnchorVisibility(isVisible: boolean) {
    this.isAnchorVisible_ = isVisible;
  }

  isAnchorFixed(): boolean {
    return this.options_.fixed;
  }

  isNonBodyScrollable(): boolean {
    return this.isNonBodyScrollable_;
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
    this.isNonBodyScrollable_ =
        !options.fixed && HelpBubbleController.getIsNonBodyScrollable_(anchor);
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
    this.bubble_.padding = this.options_.padding;

    if (params.timeout) {
      this.bubble_.timeoutMs = Number(params.timeout!.microseconds / 1000n);
      assert(this.bubble_.timeoutMs > 0);
    }

    assert(
        !this.bubble_.progress ||
        this.bubble_.progress.total >= this.bubble_.progress.current);

    assert(this.root_);

    // The bubble must be placed in the same coordinate system as the anchor.
    // The `offsetParent` of an element is the element which provides its
    // coordinate reference frame. The help bubble must also be a descendant of
    // the host (want to avoid placing the help bubble outside the mixin
    // element). This provides three possible cases:
    //  - Fixed anchor. `offsetParent` is null, coordinates are relative to the
    //    viewport. The help bubble must also be fixed.
    //  - `offsetParent` is the host or an enclosing element. The help bubble is
    //    placed in the host's shadow DOM, ensuring it shares a coordinate
    //    system (the only way this wouldn't work would be if the anchor were
    //    outside the host, which is a misuse of the help bubble system).
    //  - `offsetParent` is inside the host. The help bubble is parented to the
    //    `offsetParent`, guaranteeing that the coordinate systems are the same.
    const offsetParent = this.anchor_.offsetParent;
    const bubbleParent = (offsetParent && this.root_.contains(offsetParent)) ?
        offsetParent :
        this.root_;
    if (getComputedStyle(this.anchor_).getPropertyValue('position') ===
        'fixed') {
      this.bubble_.fixed = true;
    }
    bubbleParent.appendChild(this.bubble_);
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
      this.anchor_.focus();
      this.anchor_.scrollIntoView({behavior: 'smooth', block: 'center'});
    }
  }

  /**
   * Gets the immediate ancestor element of `element` in the DOM, or null if
   * none. This steps out of shadow DOMs as it finds them.
   */
  private static getImmediateAncestor(element: Element): Element|null {
    if (element.parentElement) {
      return element.parentElement;
    }
    if (element.parentNode instanceof ShadowRoot) {
      return (element.parentNode as ShadowRoot).host;
    }
    return null;
  }

  /**
   * Returns whether `element` has an ancestor in the document that is
   * scrollable and that is not the document body. These elements require
   * special visibility handling.
   */
  private static getIsNonBodyScrollable_(element: Element): boolean {
    const scrollableOverflow = ['scroll', 'auto', 'overlay'];
    for (let parent = HelpBubbleController.getImmediateAncestor(element);
         parent && parent !== document.body;
         parent = HelpBubbleController.getImmediateAncestor(parent)) {
      const style = getComputedStyle(parent);
      if (scrollableOverflow.includes(style.overflow) ||
          scrollableOverflow.includes(style.overflowX) ||
          scrollableOverflow.includes(style.overflowY)) {
        return true;
      }
    }
    return false;
  }
}
