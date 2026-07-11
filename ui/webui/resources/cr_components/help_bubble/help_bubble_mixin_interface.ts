// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to mixing implementations for supporting help
 * bubbles.
 *
 * See README.md for more information.
 */

import type {HelpBubbleElement} from './help_bubble.js';
import type {BrowserProxy, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import type {HelpBubbleController, HelpBubbleOptions, Trackable} from './help_bubble_controller.js';

export interface HelpBubbleMixinInterface {
  createHelpBubbleProxy(): BrowserProxy;

  /**
   * Maps `nativeId`, which should be the name of a ui::ElementIdentifier
   * referenced by the WebUIController, with either:
   * - a selector
   * - an array of selectors (will traverse shadow DOM elements)
   * - an arbitrary HTMLElement
   *
   * The referenced element should have block display and non-zero size when
   * visible (inline elements may be supported in the future).
   *
   * Example:
   *   registerHelpBubble(
   *       'kMyComponentTitleLabelElementIdentifier',
   *       '#title');
   *
   * Example:
   *   registerHelpBubble(
   *       'kMyComponentTitleLabelElementIdentifier',
   *       ['#child-component', '#child-component-button']);
   *
   * Example:
   *   registerHelpBubble(
   *       'kMyComponentTitleLabelElementIdentifier',
   *       this.$.list.childNodes[0]);
   *
   * See README.md for full instructions.
   *
   * This method can be called multiple times to re-register the nativeId to a
   * new element/selector. If the help bubble is already showing, the
   * registration will fail and return null. If successful, this method returns
   * the new controller.
   *
   * Optionally, an options object may be supplied to change the default
   * behavior of the help bubble.
   *
   * - Fixed positioning detection: e.g. `{fixed: true}`
   *   By default, this mixin detects anchor elements when rendered within the
   *   document. This breaks with fix-positioned elements since they are not in
   *   the regular flow of the document but they are always visible.
   *   Passing {"fixed": true} will detect the anchor element when it is
   *   visible.
   *
   * - Add padding around anchor element: e.g. `{paddingTop: 5}`
   *   To add to the default margin around the anchor element in all 4
   *   directions, e.g. {"paddingTop": 5} adds 5 pixels to the margin at the top
   *   off the anchor element. The margin is used when calculating how far the
   *   help bubble should be spaced from the anchor element. Larger values
   *   equate to a larger visual gap. These values must be positive integers in
   *   the range [0, 20]. This option should be used sparingly where the help
   *   bubble would otherwise conceal important UI.
   */
  registerHelpBubble(
      nativeId: string, trackable: Trackable,
      options?: HelpBubbleOptions): HelpBubbleController|null;


  /**
   * Unregisters a help bubble nativeId.
   *
   * This method will remove listeners, hide the help bubble if showing, and
   * forget the nativeId.
   */
  unregisterHelpBubble(nativeId: string): void;

  /**
   * Returns whether any help bubble is currently showing in this component.
   */
  isHelpBubbleShowing(): boolean;

  /**
   * Returns whether any help bubble is currently showing on a tag with this id.
   */
  isHelpBubbleShowingForTesting(id: string): boolean;

  /**
   * Returns the help bubble currently showing on a tag with this id.
   */
  getHelpBubbleForTesting(id: string): HelpBubbleElement|null;

  /**
   * Testing method to validate that anchors will be properly located at runtime
   *
   * Call this method in your browser_tests after your help bubbles have been
   * registered. Results are sorted to be deterministic.
   */
  getSortedAnchorStatusesForTesting(): Array<[string, boolean]>;

  /**
   * Returns whether a help bubble can be shown
   * This requires:
   * - the mixin is tracking this controller
   * - the controller is in a state to be shown, e.g.
   *   `.canShowBubble()`
   * - no other showing bubbles are anchored to the same element
   */
  canShowHelpBubble(controller: HelpBubbleController): boolean;

  /**
   * Displays a help bubble with `params` anchored to the HTML element with id
   * `anchorId`. Note that `params.nativeIdentifier` is ignored by this method,
   * since the anchor is already specified.
   */
  showHelpBubble(controller: HelpBubbleController, params: HelpBubbleParams):
      void;

  /**
   * Hides a help bubble anchored to element with id `anchorId` if there is one.
   * Returns true if a bubble was hidden.
   */
  hideHelpBubble(nativeId: string): boolean;

  /**
   * Sends an "activated" event to the ElementTracker system for the element
   * with id `anchorId`, which must have been registered as a help bubble
   * anchor. This event will be processed in the browser and may e.g. cause a
   * Tutorial or interactive test to advance to the next step.
   *
   * TODO(crbug.com/40243127): Figure out how to automatically send the
   * activated event when an anchor element is clicked.
   */
  notifyHelpBubbleAnchorActivated(anchorId: string): boolean;

  /**
   * Sends a custom event to the ElementTracker system for the element with id
   * `anchorId`, which must have been registered as a help bubble anchor. This
   * event will be processed in the browser and may e.g. cause a Tutorial or
   * interactive test to advance to the next step.
   *
   * The `customEvent` string should correspond to the name of a
   * ui::CustomElementEventType declared in the browser code.
   */
  notifyHelpBubbleAnchorCustomEvent(anchorId: string, customEvent: string):
      boolean;
}
