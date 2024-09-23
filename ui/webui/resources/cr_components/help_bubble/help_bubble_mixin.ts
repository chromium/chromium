// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to components that support a help bubble.
 *
 * A component implementing this mixin should call
 * registerHelpBubble() to associate specific element identifiers
 * referenced  in an IPH or Tutorials journey with the ids of the HTML elements
 * that journey cares about (typically, points for help bubbles to anchor to).
 *
 * Multiple components in the same WebUI may have this mixin. Each mixin will
 * receive ALL help bubble-related messages from its associated WebUIController
 * and determines if any given message is relevant. This is done by checking
 * against registered identifier.
 *
 * See README.md for more information.
 */

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import type {InsetsF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HelpBubbleDismissedEvent, HelpBubbleElement} from './help_bubble.js';
import {debounceEnd, HELP_BUBBLE_DISMISSED_EVENT, HELP_BUBBLE_TIMED_OUT_EVENT} from './help_bubble.js';
import type {HelpBubbleClientCallbackRouter, HelpBubbleHandlerInterface, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {HelpBubbleClosedReason} from './help_bubble.mojom-webui.js';
import type {Trackable} from './help_bubble_controller.js';
import {HelpBubbleController} from './help_bubble_controller.js';
import {HelpBubbleProxyImpl} from './help_bubble_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const HelpBubbleMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<HelpBubbleMixinInterface> => {
      class HelpBubbleMixin extends superClass implements
          HelpBubbleMixinInterface {
        private helpBubbleHandler_: HelpBubbleHandlerInterface;
        private helpBubbleCallbackRouter_: HelpBubbleClientCallbackRouter;
        /**
         * A map from the name of the native identifier used in the tutorial or
         * IPH definition to the target element's HTML ID.
         *
         * Example entry:
         *   "kHeightenSecuritySettingsElementId" => "toggleSecureMode"
         */
        private helpBubbleControllerById_: Map<string, HelpBubbleController> =
            new Map();
        private helpBubbleListenerIds_: number[] = [];
        private helpBubbleFixedAnchorObserver_: IntersectionObserver|null =
            null;
        private helpBubbleResizeObserver_: ResizeObserver|null = null;
        private helpBubbleDismissedEventTracker_: EventTracker =
            new EventTracker();
        private debouncedAnchorMayHaveChangedCallback_:
            (() => void)|null = null;

        constructor(...args: any[]) {
          super(...args);

          this.helpBubbleHandler_ =
              HelpBubbleProxyImpl.getInstance().getHandler();
          this.helpBubbleCallbackRouter_ =
              HelpBubbleProxyImpl.getInstance().getCallbackRouter();
        }

        override connectedCallback() {
          super.connectedCallback();

          const router = this.helpBubbleCallbackRouter_;
          this.helpBubbleListenerIds_.push(
              router.showHelpBubble.addListener(
                  this.onShowHelpBubble_.bind(this)),
              router.toggleFocusForAccessibility.addListener(
                  this.onToggleHelpBubbleFocusForAccessibility_.bind(this)),
              router.hideHelpBubble.addListener(
                  this.onHideHelpBubble_.bind(this)),
              router.externalHelpBubbleUpdated.addListener(
                  this.onExternalHelpBubbleUpdated_.bind(this)));

          const isVisible = (element: Element) => {
            const rect = element.getBoundingClientRect();
            return rect.height > 0 && rect.width > 0;
          };

          this.debouncedAnchorMayHaveChangedCallback_ =
              debounceEnd(this.onAnchorBoundsMayHaveChanged_.bind(this), 50);

          this.helpBubbleResizeObserver_ =
              new ResizeObserver(entries => entries.forEach(({target}) => {
                if (target === document.body) {
                  if (this.debouncedAnchorMayHaveChangedCallback_) {
                    this.debouncedAnchorMayHaveChangedCallback_();
                  }
                } else {
                  this.onAnchorVisibilityChanged_(
                      target as HTMLElement, isVisible(target));
                }
              }));
          this.helpBubbleFixedAnchorObserver_ = new IntersectionObserver(
              entries => entries.forEach(
                  ({target, isIntersecting}) => this.onAnchorVisibilityChanged_(
                      target as HTMLElement, isIntersecting)),
              {root: null});

          document.addEventListener(
              'scroll', this.debouncedAnchorMayHaveChangedCallback_,
              {passive: true});
          this.helpBubbleResizeObserver_.observe(document.body);

          // When the component is connected, if the target elements were
          // already registered, they should be observed now. Any targets
          // registered from this point forward will observed on registration.
          this.controllers.forEach(ctrl => this.observeControllerAnchor_(ctrl));
        }

        private get controllers(): HelpBubbleController[] {
          return Array.from(this.helpBubbleControllerById_.values());
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          for (const listenerId of this.helpBubbleListenerIds_) {
            this.helpBubbleCallbackRouter_.removeListener(listenerId);
          }
          this.helpBubbleListenerIds_ = [];
          assert(this.helpBubbleResizeObserver_);
          this.helpBubbleResizeObserver_.disconnect();
          this.helpBubbleResizeObserver_ = null;
          assert(this.helpBubbleFixedAnchorObserver_);
          this.helpBubbleFixedAnchorObserver_.disconnect();
          this.helpBubbleFixedAnchorObserver_ = null;
          this.helpBubbleDismissedEventTracker_.removeAll();
          this.helpBubbleControllerById_.clear();
          if (this.debouncedAnchorMayHaveChangedCallback_) {
            document.removeEventListener(
                'scroll', this.debouncedAnchorMayHaveChangedCallback_);
            this.debouncedAnchorMayHaveChangedCallback_ = null;
          }
        }

        /**
         * Maps `nativeId`, which should be the name of a ui::ElementIdentifier
         * referenced by the WebUIController, with either:
         * - a selector
         * - an array of selectors (will traverse shadow DOM elements)
         * - an arbitrary HTMLElement
         *
         * The referenced element should have block display and non-zero size
         * when visible (inline elements may be supported in the future).
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
         * This method can be called multiple times to re-register the
         * nativeId to a new element/selector. If the help bubble is already
         * showing, the registration will fail and return null. If successful,
         * this method returns the new controller.
         *
         * Optionally, an options object may be supplied to change the
         * default behavior of the help bubble.
         *
         * - Fixed positioning detection:
         *  e.g. `{fixed: true}`
         *  By default, this mixin detects anchor elements when
         *  rendered within the document. This breaks with
         *  fix-positioned elements since they are not in the regular
         *  flow of the document but they are always visible. Passing
         *  {"fixed": true} will detect the anchor element when it is
         *  visible.
         *
         * - Add padding around anchor element:
         *  e.g. `{anchorPaddingTop: 5}`
         *  To add to the default margin around the anchor element in all
         *  4 directions, e.g. {"anchorPaddingTop": 5} adds 5 pixels to
         *  the margin at the top off the anchor element. The margin is
         *  used when calculating how far the help bubble should be spaced
         *  from the anchor element. Larger values equate to a larger visual
         *  gap. These values must be positive integers in the range [0, 20].
         *  This option should be used sparingly where the help bubble would
         *  otherwise conceal important UI.
         */
        registerHelpBubble(
            nativeId: string, trackable: Trackable,
            options: Options = {}): HelpBubbleController|null {
          if (this.helpBubbleControllerById_.has(nativeId)) {
            const ctrl = this.helpBubbleControllerById_.get(nativeId);
            if (ctrl && ctrl.isBubbleShowing()) {
              return null;
            }
            this.unregisterHelpBubble(nativeId);
          }
          const controller =
              new HelpBubbleController(nativeId, this.shadowRoot!);
          controller.track(trackable, parseOptions(options));
          this.helpBubbleControllerById_.set(nativeId, controller);
          // This can be called before or after `connectedCallback()`, so if the
          // component isn't connected and the observer set up yet, delay
          // observation until it is.
          if (this.helpBubbleResizeObserver_) {
            this.observeControllerAnchor_(controller);
          }
          return controller;
        }

        /**
         * Unregisters a help bubble nativeId.
         *
         * This method will remove listeners, hide the help bubble if
         * showing, and forget the nativeId.
         */
        unregisterHelpBubble(nativeId: string): void {
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          if (ctrl && ctrl.hasAnchor()) {
            this.onAnchorVisibilityChanged_(ctrl.getAnchor()!, false);
            this.unobserveControllerAnchor_(ctrl);
          }
          this.helpBubbleControllerById_.delete(nativeId);
        }

        private observeControllerAnchor_(controller: HelpBubbleController) {
          const anchor = controller.getAnchor();
          assert(anchor, 'Help bubble does not have anchor');
          if (controller.isAnchorFixed()) {
            assert(this.helpBubbleFixedAnchorObserver_);
            this.helpBubbleFixedAnchorObserver_.observe(anchor);
          } else {
            assert(this.helpBubbleResizeObserver_);
            this.helpBubbleResizeObserver_.observe(anchor);
          }
        }

        private unobserveControllerAnchor_(controller: HelpBubbleController) {
          const anchor = controller.getAnchor();
          assert(anchor, 'Help bubble does not have anchor');
          if (controller.isAnchorFixed()) {
            assert(this.helpBubbleFixedAnchorObserver_);
            this.helpBubbleFixedAnchorObserver_.unobserve(anchor);
          } else {
            assert(this.helpBubbleResizeObserver_);
            this.helpBubbleResizeObserver_.unobserve(anchor);
          }
        }

        /**
         * Returns whether any help bubble is currently showing in this
         * component.
         */
        isHelpBubbleShowing(): boolean {
          return this.controllers.some(ctrl => ctrl.isBubbleShowing());
        }

        /**
         * Returns whether any help bubble is currently showing on a tag
         * with this id.
         */
        isHelpBubbleShowingForTesting(id: string): boolean {
          const ctrls =
              this.controllers.filter(this.filterMatchingIdForTesting_(id));
          return !!ctrls[0];
        }

        /**
         * Returns the help bubble currently showing on a tag with this
         * id.
         */
        getHelpBubbleForTesting(id: string): HelpBubbleElement|null {
          const ctrls =
              this.controllers.filter(this.filterMatchingIdForTesting_(id));
          return ctrls[0] ? ctrls[0].getBubble() : null;
        }

        private filterMatchingIdForTesting_(anchorId: string):
            (ctrl: HelpBubbleController) => boolean {
          return ctrl => ctrl.isBubbleShowing() && ctrl.getAnchor() !== null &&
              ctrl.getAnchor()!.id === anchorId;
        }

        /**
         * Testing method to validate that anchors will be properly
         * located at runtime
         *
         * Call this method in your browser_tests after your help
         * bubbles have been registered. Results are sorted to be
         * deterministic.
         */
        getSortedAnchorStatusesForTesting(): Array<[string, boolean]> {
          return this.controllers
              .sort((a, b) => a.getNativeId().localeCompare(b.getNativeId()))
              .map(ctrl => ([ctrl.getNativeId(), ctrl.hasAnchor()]));
        }

        /**
         * Returns whether a help bubble can be shown
         * This requires:
         * - the mixin is tracking this controller
         * - the controller is in a state to be shown, e.g.
         *   `.canShowBubble()`
         * - no other showing bubbles are anchored to the same element
         */
        canShowHelpBubble(controller: HelpBubbleController): boolean {
          if (!this.helpBubbleControllerById_.has(controller.getNativeId())) {
            return false;
          }
          if (!controller.canShowBubble()) {
            return false;
          }
          const anchor = controller.getAnchor();
          // Make sure no other help bubble is showing for this anchor.
          const anchorIsUsed = this.controllers.some(
              otherCtrl => otherCtrl.isBubbleShowing() &&
                  otherCtrl.getAnchor() === anchor);
          return !anchorIsUsed;
        }

        /**
         * Displays a help bubble with `params` anchored to the HTML element
         * with id `anchorId`. Note that `params.nativeIdentifier` is ignored by
         * this method, since the anchor is already specified.
         */
        showHelpBubble(
            controller: HelpBubbleController, params: HelpBubbleParams): void {
          assert(this.canShowHelpBubble(controller), 'Can\'t show help bubble');
          const bubble = controller.createBubble(params);

          this.helpBubbleDismissedEventTracker_.add(
              bubble, HELP_BUBBLE_DISMISSED_EVENT,
              this.onHelpBubbleDismissed_.bind(this));
          this.helpBubbleDismissedEventTracker_.add(
              bubble, HELP_BUBBLE_TIMED_OUT_EVENT,
              this.onHelpBubbleTimedOut_.bind(this));

          controller.show();
        }

        /**
         * Hides a help bubble anchored to element with id `anchorId` if there
         * is one. Returns true if a bubble was hidden.
         */
        hideHelpBubble(nativeId: string): boolean {
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          if (!ctrl || !ctrl.hasBubble()) {
            // `!ctrl` means this identifier is not handled by this mixin
            return false;
          }

          this.helpBubbleDismissedEventTracker_.remove(
              ctrl.getBubble()!, HELP_BUBBLE_DISMISSED_EVENT);
          this.helpBubbleDismissedEventTracker_.remove(
              ctrl.getBubble()!, HELP_BUBBLE_TIMED_OUT_EVENT);

          ctrl.hide();
          return true;
        }

        /**
         * Sends an "activated" event to the ElementTracker system for the
         * element with id `anchorId`, which must have been registered as a help
         * bubble anchor. This event will be processed in the browser and may
         * e.g. cause a Tutorial or interactive test to advance to the next
         * step.
         *
         * TODO(crbug.com/40243127): Figure out how to automatically send the
         * activated event when an anchor element is clicked.
         */
        notifyHelpBubbleAnchorActivated(nativeId: string): boolean {
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          if (!ctrl || !ctrl.isBubbleShowing()) {
            return false;
          }
          this.helpBubbleHandler_.helpBubbleAnchorActivated(nativeId);
          return true;
        }

        /**
         * Sends a custom event to the ElementTracker system for the element
         * with id `anchorId`, which must have been registered as a help bubble
         * anchor. This event will be processed in the browser and may e.g.
         * cause a Tutorial or interactive test to advance to the next step.
         *
         * The `customEvent` string should correspond to the name of a
         * ui::CustomElementEventType declared in the browser code.
         */
        notifyHelpBubbleAnchorCustomEvent(
            nativeId: string, customEvent: string): boolean {
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          if (!ctrl || !ctrl.isBubbleShowing()) {
            return false;
          }
          this.helpBubbleHandler_.helpBubbleAnchorCustomEvent(
              nativeId, customEvent);
          return true;
        }

        /**
         * This event is emitted by the mojo router
         */
        private onAnchorVisibilityChanged_(
            target: HTMLElement, isVisible: boolean) {
          const nativeId = target.dataset['nativeId'];
          assert(nativeId);
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          const hidden = this.hideHelpBubble(nativeId);
          if (hidden) {
            this.helpBubbleHandler_.helpBubbleClosed(
                nativeId, HelpBubbleClosedReason.kPageChanged);
          }
          const bounds: RectF = isVisible ? this.getElementBounds_(target) :
                                            {x: 0, y: 0, width: 0, height: 0};
          if (!ctrl || ctrl.updateAnchorVisibility(isVisible, bounds)) {
            this.helpBubbleHandler_.helpBubbleAnchorVisibilityChanged(
                nativeId, isVisible, bounds);
          }
        }

        /**
         * When the document scrolls or resizes, we need to update cached
         * positions of bubble anchors.
         */
        private onAnchorBoundsMayHaveChanged_() {
          for (const ctrl of this.controllers) {
            if (ctrl.hasAnchor() && ctrl.getAnchorVisibility()) {
              const bounds = this.getElementBounds_(ctrl.getAnchor()!);
              if (ctrl.updateAnchorVisibility(true, bounds)) {
                this.helpBubbleHandler_.helpBubbleAnchorVisibilityChanged(
                    ctrl.getNativeId(), true, bounds);
              }
            }
          }
        }

        /**
         * Returns bounds of the anchor element
         */
        private getElementBounds_(element: HTMLElement) {
          const rect: RectF = {x: 0, y: 0, width: 0, height: 0};
          const bounds = element.getBoundingClientRect();
          rect.x = bounds.x;
          rect.y = bounds.y;
          rect.width = bounds.width;
          rect.height = bounds.height;
          const nativeId = element.dataset['nativeId'];
          if (!nativeId) {
            return rect;
          }
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          if (ctrl) {
            const padding = ctrl.getPadding();
            rect.x -= padding.left;
            rect.y -= padding.top;
            rect.width += padding.left + padding.right;
            rect.height += padding.top + padding.bottom;
          }
          return rect;
        }

        /**
         * This event is emitted by the mojo router
         */
        private onShowHelpBubble_(params: HelpBubbleParams): void {
          if (!this.helpBubbleControllerById_.has(params.nativeIdentifier)) {
            // Identifier not handled by this mixin.
            return;
          }
          const ctrl =
              this.helpBubbleControllerById_.get(params.nativeIdentifier)!;
          this.showHelpBubble(ctrl, params);
        }

        /**
         * This event is emitted by the mojo router
         */
        private onToggleHelpBubbleFocusForAccessibility_(nativeId: string) {
          if (!this.helpBubbleControllerById_.has(nativeId)) {
            // Identifier not handled by this mixin.
            return;
          }

          const ctrl = this.helpBubbleControllerById_.get(nativeId)!;
          if (ctrl) {
            const anchor = ctrl.getAnchor();
            if (anchor) {
              anchor.focus();
            }
          }
        }

        /**
         * This event is emitted by the mojo router
         */
        private onHideHelpBubble_(nativeId: string): void {
          // This may be called with nativeId not handled by this mixin
          // Ignore return value to silently fail
          this.hideHelpBubble(nativeId);
        }

        /**
         * This event is emitted by the mojo router.
         */
        private onExternalHelpBubbleUpdated_(nativeId: string, shown: boolean) {
          if (!this.helpBubbleControllerById_.has(nativeId)) {
            // Identifier not handled by this mixin.
            return;
          }

          // Get the associated bubble and update status
          const ctrl = this.helpBubbleControllerById_.get(nativeId)!;
          ctrl.updateExternalShowingStatus(shown);
        }

        /**
         * This event is emitted by the help-bubble component
         */
        private onHelpBubbleDismissed_(e: HelpBubbleDismissedEvent) {
          const nativeId = e.detail.nativeId;
          assert(nativeId);
          const hidden = this.hideHelpBubble(nativeId);
          assert(hidden);
          if (nativeId) {
            if (e.detail.fromActionButton) {
              this.helpBubbleHandler_.helpBubbleButtonPressed(
                  nativeId, e.detail.buttonIndex!);
            } else {
              this.helpBubbleHandler_.helpBubbleClosed(
                  nativeId, HelpBubbleClosedReason.kDismissedByUser);
            }
          }
        }

        /**
         * This event is emitted by the help-bubble component
         */
        private onHelpBubbleTimedOut_(e: HelpBubbleDismissedEvent) {
          const nativeId = e.detail.nativeId;
          const ctrl = this.helpBubbleControllerById_.get(nativeId);
          assert(ctrl);
          const hidden = this.hideHelpBubble(nativeId);
          assert(hidden);
          if (nativeId) {
            this.helpBubbleHandler_.helpBubbleClosed(
                nativeId, HelpBubbleClosedReason.kTimedOut);
          }
        }
      }

      return HelpBubbleMixin;
    });

export interface HelpBubbleMixinInterface {
  registerHelpBubble(nativeId: string, trackable: Trackable, options?: Options):
      HelpBubbleController|null;
  unregisterHelpBubble(nativeId: string): void;
  isHelpBubbleShowing(): boolean;
  isHelpBubbleShowingForTesting(id: string): boolean;
  getHelpBubbleForTesting(id: string): HelpBubbleElement|null;
  getSortedAnchorStatusesForTesting(): Array<[string, boolean]>;
  canShowHelpBubble(controller: HelpBubbleController): boolean;
  showHelpBubble(controller: HelpBubbleController, params: HelpBubbleParams):
      void;
  hideHelpBubble(nativeId: string): boolean;
  notifyHelpBubbleAnchorActivated(anchorId: string): boolean;
  notifyHelpBubbleAnchorCustomEvent(anchorId: string, customEvent: string):
      boolean;
}

export interface Options {
  anchorPaddingTop?: number;
  anchorPaddingLeft?: number;
  anchorPaddingBottom?: number;
  anchorPaddingRight?: number;
  fixed?: boolean;
}

export function parseOptions(options: Options) {
  const padding: InsetsF = {top: 0, bottom: 0, left: 0, right: 0};
  padding.top = clampPadding(options.anchorPaddingTop);
  padding.left = clampPadding(options.anchorPaddingLeft);
  padding.bottom = clampPadding(options.anchorPaddingBottom);
  padding.right = clampPadding(options.anchorPaddingRight);
  return {
    padding,
    fixed: !!options.fixed,
  };
}

function clampPadding(n: number = 0) {
  return Math.max(0, Math.min(20, n));
}
