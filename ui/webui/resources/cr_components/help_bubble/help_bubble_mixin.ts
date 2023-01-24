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

import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {InsetsF, RectF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {debounceEnd, HELP_BUBBLE_DISMISSED_EVENT, HELP_BUBBLE_TIMED_OUT_EVENT, HelpBubbleDismissedEvent, HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleClosedReason, HelpBubbleHandlerInterface, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {HelpBubbleController, Trackable} from './help_bubble_controller.js';
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
        private helpBubbleAnchorObserver_: IntersectionObserver|null = null;
        private helBubbleDismissedEventTracker_: EventTracker =
            new EventTracker();
        private helpBubbleScrollCallbackDebounced_:
            EventListenerOrEventListenerObject|null = null;

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

          this.helpBubbleAnchorObserver_ = new IntersectionObserver(
              entries => entries.forEach(
                  ({target, isIntersecting}) => this.onAnchorVisibilityChanged_(
                      target as HTMLElement, isIntersecting)),
              {root: document.body});

          this.helpBubbleScrollCallbackDebounced_ =
              debounceEnd(this.helpBubbleScrollCallback_.bind(this), 50) as
              EventListenerOrEventListenerObject;
          document.addEventListener(
              'scroll', this.helpBubbleScrollCallbackDebounced_,
              {passive: true});

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
          assert(this.helpBubbleAnchorObserver_);
          this.helpBubbleAnchorObserver_.disconnect();
          this.helpBubbleAnchorObserver_ = null;
          this.helpBubbleControllerById_.clear();
          if (this.helpBubbleScrollCallbackDebounced_) {
            document.removeEventListener(
                'scroll', this.helpBubbleScrollCallbackDebounced_);
            this.helpBubbleScrollCallbackDebounced_ = null;
          }
        }

        /**
         * Maps `nativeId`, which should be the name of a ui::ElementIdentifier
         * referenced by the WebUIController, with either:
         * - a selector
         * - an array of selectors (will traverse shadow DOM elements)
         * - an arbitrary HTMLElement
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
         * Optionally, an object may be supplied to add to the default margin
         * around the anchor element in all 4 directions, e.g. {"top":5}
         * adds 5 pixels to the margin at the top off the anchor element.
         * The margin is used when calculating how far the help bubble should
         * be spaced from the anchor element. Larger values equate to a larger
         * visual gap. These values must be positive integers in the range
         * [0, 20]. This option should be used sparingly where the help
         * bubble would otherwise conceal important UI.
         */
        registerHelpBubble(
            nativeId: string, trackable: Trackable,
            padding: Padding = {}): HelpBubbleController|null {
          if (this.helpBubbleControllerById_.has(nativeId)) {
            const ctrl = this.helpBubbleControllerById_.get(nativeId);
            if (ctrl && ctrl.isBubbleShowing()) {
              return null;
            }
            this.unregisterHelpBubble(nativeId);
          }
          const controller =
              new HelpBubbleController(nativeId, this.shadowRoot!);
          controller.track(trackable, paddingToInsets(padding));
          this.helpBubbleControllerById_.set(nativeId, controller);
          // This can be called before or after `connectedCallback()`, so if the
          // component isn't connected and the observer set up yet, delay
          // observation until it is.
          if (this.helpBubbleAnchorObserver_) {
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
          assert(this.helpBubbleAnchorObserver_);
          const anchor = controller.getAnchor();
          assert(anchor, 'Help bubble does not have anchor');
          this.helpBubbleAnchorObserver_.observe(anchor);
        }

        private unobserveControllerAnchor_(controller: HelpBubbleController) {
          assert(this.helpBubbleAnchorObserver_);
          const anchor = controller.getAnchor();
          assert(anchor, 'Help bubble does not have anchor');
          this.helpBubbleAnchorObserver_.unobserve(anchor);
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

          this.helBubbleDismissedEventTracker_.add(
              bubble, HELP_BUBBLE_DISMISSED_EVENT,
              this.onHelpBubbleDismissed_.bind(this));
          this.helBubbleDismissedEventTracker_.add(
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

          this.helBubbleDismissedEventTracker_.remove(
              ctrl.getBubble()!, HELP_BUBBLE_DISMISSED_EVENT);
          this.helBubbleDismissedEventTracker_.remove(
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
         * TODO(crbug.com/1376262): Figure out how to automatically send the
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
          if (ctrl) {
            ctrl.cacheAnchorVisibility(isVisible);
          }
          const hidden = this.hideHelpBubble(nativeId);
          if (hidden) {
            this.helpBubbleHandler_.helpBubbleClosed(
                nativeId, HelpBubbleClosedReason.kPageChanged);
          }
          const bounds =
              isVisible ? this.getElementBounds_(target) : new RectF();
          this.helpBubbleHandler_.helpBubbleAnchorVisibilityChanged(
              nativeId, isVisible, bounds);
        }

        /**
         * When the document scrolls, we need to update cached positions
         * of bubble anchors
         */
        private helpBubbleScrollCallback_() {
          for (const ctrl of this.controllers) {
            if (ctrl.hasAnchor() && ctrl.getAnchorVisibility()) {
              this.helpBubbleHandler_.helpBubbleAnchorVisibilityChanged(
                  ctrl.getNativeId(), ctrl.getAnchorVisibility(),
                  this.getElementBounds_(ctrl.getAnchor()!));
            }
          }
        }

        /**
         * Returns bounds of the anchor element
         */
        private getElementBounds_(element: HTMLElement) {
          const rect = new RectF();
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
  registerHelpBubble(nativeId: string, trackable: Trackable, padding?: Padding):
      HelpBubbleController|null;
  unregisterHelpBubble(nativeId: string): void;
  isHelpBubbleShowing(): boolean;
  isHelpBubbleShowingForTesting(id: string): boolean;
  getHelpBubbleForTesting(id: string): HelpBubbleElement|null;
  canShowHelpBubble(controller: HelpBubbleController): boolean;
  showHelpBubble(controller: HelpBubbleController, params: HelpBubbleParams):
      void;
  hideHelpBubble(nativeId: string): boolean;
  notifyHelpBubbleAnchorActivated(anchorId: string): boolean;
  notifyHelpBubbleAnchorCustomEvent(anchorId: string, customEvent: string):
      boolean;
}

export interface Padding {
  top?: number;
  left?: number;
  bottom?: number;
  right?: number;
}

export function paddingToInsets(padding: Padding) {
  const insets = new InsetsF();
  insets.top = clampPadding(padding.top);
  insets.left = clampPadding(padding.left);
  insets.bottom = clampPadding(padding.bottom);
  insets.right = clampPadding(padding.right);
  return insets;
}

function clampPadding(n: number = 0) {
  return Math.max(0, Math.min(20, n));
}
