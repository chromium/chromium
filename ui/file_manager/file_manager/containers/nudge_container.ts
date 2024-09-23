// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../widgets/xf_nudge.js';

import {storage} from '../common/js/storage.js';
import {str} from '../common/js/translations.js';
import type {XfNudge} from '../widgets/xf_nudge.js';
import {NudgeDirection} from '../widgets/xf_nudge.js';
import type {XfTreeItem} from '../widgets/xf_tree_item.js';

/**
 * NudgeContainer maintains the lifetime of a "nudge". A nudge refers to an
 * educational overlay that shows up to highlight new features, currently we
 * only support a single nudge showing in Files app.
 */
export class NudgeContainer {
  /**
   * The educational nudge that is added as a web component to the DOM.
   */
  private nudge_: XfNudge = document.querySelector('xf-nudge')!;

  /**
   * The handle that represents the requestIdleCallback to enable cancellation.
   */
  private idleCallbackHandle_: number = -1;

  /**
   * The current `NudgeType` that is visible.
   */
  private currentNudgeType_: NudgeType|undefined = undefined;

  /**
   * Each nudge has a described-by <p> tag to enable an announcement to be made
   * when hovering over or tabbing to the anchored element.
   */
  private readonly anchorAriaDescribedbyElement_ = document.createElement('p');

  /**
   * A controller which sends out an abort signal once we no longer want to
   * listen to events i.e. on nudge hide.
   */
  private listenerAbortController_: AbortController|null = null;

  /**
   * Cache the DOMRect of the anchor to allow comparison of the previous
   * location and in the case the anchor DOMRect changes, reposition the nudge
   * accordingly.
   */
  private anchorDomRect_: DOMRect|undefined = undefined;

  /**
   * Stores the ID of the current requestAnimationFrame(). Used to ensure only
   * run callback is running at a time.
   */
  private requestAnimationFrameId_: number|undefined = undefined;

  /**
   * True if the expiry period on the nudge is observed. False otherwise.
   */
  private expiryPeriodEnabled_: boolean = true;

  constructor() {
    this.anchorAriaDescribedbyElement_.id = 'nudge-content';
    this.anchorAriaDescribedbyElement_.style.display = 'none';
  }

  /**
   * A callback that repositions the nudge element prior to a repaint. The
   * callback is throttled to only run on animation frames since we call it for
   * scroll events; which can be numerous between frames.
   */
  private throttledRepositionCallback_() {
    if (this.requestAnimationFrameId_) {
      return;
    }

    this.requestAnimationFrameId_ = window.requestAnimationFrame(() => {
      if (!this.nudgeShowing_) {
        return;
      }

      const anchorDomRect = this.nudge_.anchor!.getBoundingClientRect();
      // First verify that the anchor has changed in some position or dimension
      // before repositioning the nudge. This ensures we're not too aggressive
      // in repositioning.
      if (this.anchorDomRect_ &&
          (anchorDomRect.x !== this.anchorDomRect_.x ||
           anchorDomRect.y !== this.anchorDomRect_.y ||
           anchorDomRect.width !== this.anchorDomRect_.width ||
           anchorDomRect.height !== this.anchorDomRect_.height)) {
        this.anchorDomRect_ = anchorDomRect;
        this.nudge_.reposition();
      }

      this.requestAnimationFrameId_ = undefined;
    });
  }

  /**
   * Attempts to reposition the visible nudge if it is showing. There is no easy
   * way to listen for DOM elements that change without user input (e.g. if a
   * volume is added or removed from the directory tree). So use an IdleCallback
   * to keep checking the nudge is in the right position.
   */
  private idleCallback_() {
    if (this.nudgeShowing_) {
      this.throttledRepositionCallback_();
      this.idleCallbackHandle_ = window.requestIdleCallback(
          this.idleCallback_.bind(this), {timeout: 1000});
      return;
    }

    window.cancelIdleCallback(this.idleCallbackHandle_);
  }

  /**
   * A method for the nudge manager to decide whether a given nudge has been
   * previously seen and dismissed by the user.
   */
  async checkSeen(nudgeId: string) {
    const seen = await storage.local.getAsync(nudgeId);
    return seen[nudgeId] === 'true';
  }

  /**
   * A method for the nudge manager to specify that a given nudge has been seen
   * and dismissed by the user.
   */
  async setSeen(nudgeId: string) {
    return storage.local.setAsync({[nudgeId]: 'true'});
  }

  /**
   * Clears the `seen` state from the localStorage for the given nudge.
   */
  async clearSeen(nudgeType: NudgeType) {
    storage.local.remove(nudgeType);
  }

  /**
   * Shows the nudge if it has not already been seen before.
   */
  async showNudge(nudge: NudgeType) {
    if (this.nudgeShowing_) {
      return;
    }

    // No nudge info exists for the supplied nudge.
    if (!nudgeInfo[nudge]) {
      console.warn('Nudge', nudge, 'does not exist');
      return;
    }
    if (!nudgeInfo[nudge].anchor()) {
      console.warn('nudge anchor', nudge, 'does not exist');
      return;
    }
    const info = nudgeInfo[nudge];
    const anchor = info.anchor() as HTMLElement;

    // Don't show the nudge if it's expired and the expiry period is enabled.
    if (info.expiryDate && info.expiryDate < new Date() &&
        this.expiryPeriodEnabled_) {
      return;
    }

    if (await this.checkSeen(nudge)) {
      return;
    }

    this.currentNudgeType_ = nudge;

    // Create a new controller since they can only be aborted once (adding an
    // aborted signal to a listener will result in no listening).
    this.listenerAbortController_ = new AbortController();
    // Anchor container scrolling and document resizes can potentially
    // reposition the anchor, which will need a matching reposition of the nudge
    // element-- so we listen to those events and reposition upon them
    // occurring. Note, it is possible that there are other ways of manipulating
    // the anchor position without triggering any of the events here (e.g.
    // resizing an element within the document); but no such use case exists
    // yet.
    const config = {
      signal: this.listenerAbortController_.signal,
      passive: true,
    };
    let anchorTreeNode: Node|null = anchor;
    while (anchorTreeNode) {
      if (anchorTreeNode instanceof EventTarget) {
        anchorTreeNode.addEventListener(
            'scroll', this.throttledRepositionCallback_.bind(this), config);
      }
      anchorTreeNode = anchorTreeNode.parentNode;
      if (anchorTreeNode instanceof ShadowRoot) {
        anchorTreeNode = anchorTreeNode.host;
      }
    }
    window.addEventListener(
        'resize', this.throttledRepositionCallback_.bind(this), config);

    if (info.selfDismiss) {
      // Self dismissable nudge only dismisses if the user clicks on the nudge.
      this.nudge_.addEventListener(
          'pointerdown', () => this.closeNudge(this.currentNudgeType_), config);
      anchor.addEventListener(
          'pointerdown', () => this.closeNudge(this.currentNudgeType_), config);
      const dismissOnKeyDown = info.dismissOnKeyDown;
      if (dismissOnKeyDown) {
        document.addEventListener('keydown', (event: KeyboardEvent) => {
          if (dismissOnKeyDown(anchor, event)) {
            this.closeNudge(this.currentNudgeType_);
          }
        }, config);
      }
    } else {
      // Otherwise the nudge dismisses when user clicks anywhere in the app.
      document.addEventListener('keydown', e => this.handleKeyDown_(e), config);
      document.addEventListener(
          'pointerdown', e => this.handlePointerDown_(e), config);
      anchor.addEventListener(
          'blur', (_: Event) => this.closeNudge(this.currentNudgeType_),
          config);
      this.nudge_.dismissText = '';
    }

    this.nudge_.anchor = anchor;
    this.nudge_.content = info.content();
    this.nudge_.direction = info.direction;

    this.nudge_.show();
    this.anchorDomRect_ = anchor.getBoundingClientRect();
    this.setAnchorAriaDescribedby_();

    window.cancelIdleCallback(this.idleCallbackHandle_);
    this.idleCallbackHandle_ = window.requestIdleCallback(
        this.idleCallback_.bind(this), {timeout: 1000});
  }

  /**
   * Hide the currently showing nudge and update the seen status if provided.
   */
  async closeNudge(seenNudgeId?: string) {
    window.cancelIdleCallback(this.idleCallbackHandle_);
    if (!this.nudgeShowing_) {
      return;
    }
    if (seenNudgeId) {
      await this.setSeen(seenNudgeId);
    }
    this.nudge_.hide();
    this.currentNudgeType_ = undefined;
    this.anchorDomRect_ = undefined;
    this.removeAnchorAriaDescribedby_();
    // Abort listeners since we don't want to update position after hiding.
    this.listenerAbortController_?.abort();
  }

  /**
   * Used to override the expiry period for nudges in test.
   */
  set setExpiryPeriodEnabledForTesting(value: boolean) {
    this.expiryPeriodEnabled_ = value;
  }

  /**
   * Handle key down events such that any "Escape", "Enter" or "Space" should
   * close the nudge.
   */
  private handleKeyDown_(event: KeyboardEvent) {
    switch (event.key) {
      case 'Escape':
      case 'Enter':
      case 'Space':
        this.closeNudge(this.currentNudgeType_);
        break;
      default:
        break;
    }
  }

  /**
   * Handle any pointer down events on the Nudge.
   */
  private handlePointerDown_(event: MouseEvent) {
    // Ignore pointer events on the nudge to allow copying the nudge's text.
    if (event.composedPath().includes(this.nudge_)) {
      return;
    }
    this.closeNudge(this.currentNudgeType_);
  }

  /**
   * Set the <p> aria-described-by content to enable screen readers to hear the
   * nudge content when navigating over the anchored element.
   */
  private setAnchorAriaDescribedby_() {
    if (!this.nudge_.anchor) {
      return;
    }

    this.anchorAriaDescribedbyElement_.innerText = this.nudge_.content;
    // Add a new element as a sibling of the anchor so we can aria-describedBy
    // it to read out the contents of the nudge.
    this.nudge_.anchor.insertAdjacentElement(
        'afterend', this.anchorAriaDescribedbyElement_);
    this.nudge_.anchor.setAttribute('aria-describedby', 'nudge-content');
  }

  /**
   * Remove the <p> aria-described-by content.
   */
  private removeAnchorAriaDescribedby_() {
    this.anchorAriaDescribedbyElement_.remove();

    if (!this.nudge_.anchor) {
      return;
    }

    this.nudge_.anchor.removeAttribute('aria-describedby');
  }

  /**
   * Helper function to return whether a current nudge is showing.
   */
  private get nudgeShowing_() {
    return this.currentNudgeType_ !== undefined;
  }
}

/**
 * An enum of nudges that can be shown, only a single nudge is shown at a time.
 */
export enum NudgeType {
  TEST_NUDGE = 'test-nudge',
  MANUAL_TEST_NUDGE = 'manual-test-nudge',
  ONE_DRIVE_MOVED_FILE_NUDGE = 'one-drive-moved-file-nudge',
  DRIVE_MOVED_FILE_NUDGE = 'drive-moved-file-nudge',
  SEARCH_V2_EDUCATION_NUDGE = 'search-v2-education-nudge',
}

/**
 * Type to define the callback used that gets the anchor element from the DOM.
 */
interface NudgeInfo {
  // The anchor that the nudge will appear near. The location of the nudge
  // relative to the anchor is dictated by the `direction`.
  anchor: () => HTMLElement | null;

  // The string contents of the nudge.
  content: () => string;

  // The direction that nudge appears relative to the anchor. For more
  // explanation on the various `NudgeDirection`'s look in `xf_nudge.ts` file.
  direction: NudgeDirection;

  // The date the nudge expires, after this date even if the nudge is invoked it
  // will not appear.
  expiryDate: Date;

  // When the using selfDimiss=true the user can dismiss by clicking in the
  // nudge. Otherwise the nudge is dismissed when clicking anywhere in
  // the app/document.
  selfDismiss?: boolean;

  // For selfDismiss nudge the nudge and its anchor might not get keyboard focus
  // to be able to dismiss via keyboard.
  // Implement this callback that receives the keydown from document and should
  // return true if the nudge should be dismissed.
  dismissOnKeyDown?:
      (anchor: HTMLElement|null, event: KeyboardEvent) => boolean;
}

/**
 * Dismisses the nudge when the tree-item that anchors the nudge is selected.
 *
 * NOTE: It relies on the nudge anchor being in the icon, to traverse 2 parents
 * up to the tree-item.
 */
function treeDismissOnKeyDownOnTreeItem(
    anchor: HTMLElement|null, event: KeyboardEvent) {
  const dismissKeys = new Set(['Enter', 'Space']);
  if (!dismissKeys.has(event.key)) {
    return false;
  }

  // When the anchor (tree item) is selected we dismiss.
  const parentTreeItem: Element|null|undefined =
      (anchor?.getRootNode() as ShadowRoot)?.host;
  if (parentTreeItem?.hasAttribute('selected')) {
    return true;
  }
  return false;
}

/**
 * A mapping of nudges to their information that can be shown throughout the
 * Files app.
 */
export const nudgeInfo: {[type in NudgeType]: NudgeInfo} = {
  [NudgeType['TEST_NUDGE']]: {
    anchor: () => document.querySelector<HTMLDivElement>('div#test'),
    content: () => 'Test content',
    direction: NudgeDirection.BOTTOM_ENDWARD,
    expiryDate: new Date(2999, 1, 1),
  },
  [NudgeType['MANUAL_TEST_NUDGE']]: {
    anchor: () => {
      const downloadsTreeItem =
          document.querySelector<XfTreeItem>('xf-tree-item[icon="downloads"]')!;
      return downloadsTreeItem.shadowRoot!.querySelector('xf-icon');
    },
    content: () => str('ONE_DRIVE_MOVED_FILE_NUDGE'),
    direction: NudgeDirection.TRAILING_DOWNWARD,
    expiryDate: new Date(2999, 1, 1),
    selfDismiss: true,
    dismissOnKeyDown: treeDismissOnKeyDownOnTreeItem,
  },
  [NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE']]: {
    anchor: () => {
      const oneDriveTreeItem =
          document.querySelector<XfTreeItem>('xf-tree-item[one-drive]');
      return oneDriveTreeItem?.shadowRoot!.querySelector('.tree-row') || null;
    },
    content: () => str('ONE_DRIVE_MOVED_FILE_NUDGE'),
    direction: NudgeDirection.TRAILING_DOWNWARD,
    expiryDate: new Date(2025, 12, 5),
    selfDismiss: true,
    dismissOnKeyDown: treeDismissOnKeyDownOnTreeItem,
  },
  [NudgeType['DRIVE_MOVED_FILE_NUDGE']]: {
    anchor: () => {
      const driveTreeItem = document.querySelector<XfTreeItem>(
          'xf-tree-item[icon="service_drive"]');
      return driveTreeItem?.shadowRoot!.querySelector('.tree-row') || null;
    },
    content: () => str('DRIVE_MOVED_FILE_NUDGE'),
    direction: NudgeDirection.TRAILING_DOWNWARD,
    expiryDate: new Date(2025, 12, 5),
    selfDismiss: true,
    dismissOnKeyDown: treeDismissOnKeyDownOnTreeItem,
  },
  [NudgeType['SEARCH_V2_EDUCATION_NUDGE']]: {
    anchor: () =>
        document.querySelector<HTMLDivElement>('#search-button > .icon'),
    content: () => str('SEARCH_V2_EDUCATION_NUDGE'),
    direction: NudgeDirection.BOTTOM_STARTWARD,
    // Expire after 4 releases (expires when M120 hits Stable).
    expiryDate: new Date(2023, 12, 5),
  },
};
