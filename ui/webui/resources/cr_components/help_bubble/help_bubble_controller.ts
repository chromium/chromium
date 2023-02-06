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
 * Function used to test ancestor elements
 *
 * Returning true signals we want to return this element
 * Returning false will continue to traverse ancestors
 */
type ElementTestFn = (element: Element) => boolean;

/**
 * Traverse ancestor elements to find a parent that passes the `testFn`,
 * crossing shadowRoot boundaries when encountered
 *
 * The traversal starts with the parent of the `child` element
 *
 * Traversal ends either when the `root` shadowRoot is encountered or
 * an element produces `true` from the `testFn`.
 */
function getAncestor(child: Element, root: ShadowRoot, testFn: ElementTestFn):
    Element|ShadowRoot {
  let parent: Element|ShadowRoot|null = child.parentElement;
  while (parent && !testFn(parent)) {
    if (!parent.parentElement && parent.parentNode instanceof ShadowRoot) {
      if (parent.parentNode === root) {
        return root;
      }
      parent = parent.parentNode.host;
    } else {
      parent = parent.parentElement;
    }
  }
  return parent ? parent : root;
}

const relativePositionValues = [
  'absolute',
  'fixed',
  'relative',
];

const isRelativePosition: ElementTestFn = (element) => {
  const positionVal = getComputedStyle(element).getPropertyValue('position');
  return relativePositionValues.includes(positionVal);
};

const isFixedPosition: ElementTestFn = (element) => {
  return getComputedStyle(element).getPropertyValue('position') === 'fixed';
};

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
    this.anchor_.focus();
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
    if (this.options_.fixed) {
      if (isFixedPosition(this.anchor_)) {
        // anchor is fixed so bubble needs to be fixed
        // insert bubble into root
        this.bubble_.fixed = true;
        const relativeRoot =
            getAncestor(this.anchor_, this.root_, isRelativePosition);
        relativeRoot.appendChild(this.bubble_);
      } else {
        // ancestor of anchor should be fixed
        // insert bubble into fixed ancestor
        const fixedRoot =
            getAncestor(this.anchor_, this.root_, isFixedPosition);
        fixedRoot.appendChild(this.bubble_);
      }
    } else {
      // insert bubble into root
      this.root_.appendChild(this.bubble_);
    }

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
  }
}
