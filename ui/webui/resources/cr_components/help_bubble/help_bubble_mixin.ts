// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to components that support a help bubble.
 *
 * A component implementing this mixin should call
 * registerHelpBubbleIdentifier() to associate specific element identifiers
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
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HELP_BUBBLE_DISMISSED_EVENT, HELP_BUBBLE_TIMED_OUT_EVENT, HelpBubbleDismissedEvent, HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleClosedReason, HelpBubbleHandlerInterface, HelpBubbleParams} from './help_bubble.mojom-webui.js';
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
        private helpBubbleNativeToTargetId_: Map<string, string> = new Map();
        private listenerIds_: number[] = [];
        private helpBubbleTargetObserver_: IntersectionObserver|null = null;
        /**
         * Tracks the last known visibility of any element registered via
         * `registerHelpBubbleIdentifier()` and tracked by
         * `helpBubbleTargetObserver_`. Maps from HTML id to last known
         * visibility.
         */
        private targetVisibility_: Map<string, boolean> = new Map();
        private dismissedEventTracker_: EventTracker = new EventTracker();

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
          this.listenerIds_.push(
              router.showHelpBubble.addListener(
                  this.onShowHelpBubble_.bind(this)),
              router.toggleFocusForAccessibility.addListener(
                  this.onToggleHelpBubbleFocusForAccessibility_.bind(this)),
              router.hideHelpBubble.addListener(
                  this.onHideHelpBubble_.bind(this)));

          this.helpBubbleTargetObserver_ =
              new IntersectionObserver((entries, _observer) => {
                for (const entry of entries) {
                  this.onTargetVisibilityChanged_(
                      entry.target, entry.isIntersecting);
                }
              }, {root: document.body});

          // When the component is connected, if the target elements were
          // already registered, they should be observed now. Any targets
          // registered from this point forward will observed on registration.
          for (const htmlId of this.helpBubbleNativeToTargetId_.values()) {
            this.observeTarget_(htmlId);
          }
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          for (const listenerId of this.listenerIds_) {
            this.helpBubbleCallbackRouter_.removeListener(listenerId);
          }
          this.listenerIds_ = [];
          assert(this.helpBubbleTargetObserver_);
          this.helpBubbleTargetObserver_.disconnect();
          this.helpBubbleTargetObserver_ = null;
          this.helpBubbleNativeToTargetId_.clear();
          this.targetVisibility_.clear();
        }

        /**
         * Maps `nativeId`, which should be the name of a ui::ElementIdentifier
         * referenced by the WebUIController, with the `htmlId` of an element in
         * this component.
         *
         * Example:
         *   registerHelpBubbleIdentifier(
         *       'kMyComponentTitleLabelElementIdentifier',
         *       'title');
         *
         * See README.md for full instructions.
         */
        registerHelpBubbleIdentifier(nativeId: string, htmlId: string): void {
          assert(!this.helpBubbleNativeToTargetId_.has(nativeId));
          this.helpBubbleNativeToTargetId_.set(nativeId, htmlId);
          // This can be called before or after `connectedCallback()`, so if the
          // component isn't connected and the observer set up yet, delay
          // observation until it is.
          if (!this.helpBubbleTargetObserver_) {
            return;
          }
          this.observeTarget_(htmlId);
        }

        /**
         * Returns whether any help bubble is currently showing in this
         * component.
         */
        isHelpBubbleShowing(): boolean {
          return !!this.shadowRoot!.querySelector('help-bubble');
        }

        /**
         * Returns whether a help bubble anchored to element with HTML id
         * `anchorId` is currently showing.
         */
        isHelpBubbleShowingFor(anchorId: string): boolean {
          return !!this.getHelpBubbleFor_(anchorId);
        }

        /**
         * Displays a help bubble with `params` anchored to the HTML element
         * with id `anchorId`. Note that `params.nativeIdentifier` is ignored by
         * this method, since the anchor is already specified.
         */
        showHelpBubble(anchorId: string, params: HelpBubbleParams): void {
          const oldBubble = this.getHelpBubbleFor_(anchorId);
          assert(
              !oldBubble,
              'Can\'t show help bubble; ' +
                  'bubble already showing and anchored to ' + anchorId);

          const bubble = document.createElement('help-bubble');
          const anchor = this.findAnchorElement_(anchorId);
          assert(anchor, 'Help bubble anchor element not found ' + anchorId);

          // insert after anchor - if nextSibling is null, bubble will
          // be added as the last child of parentNode
          anchor.parentNode!.insertBefore(bubble, anchor.nextSibling);

          this.dismissedEventTracker_.add(
              bubble, HELP_BUBBLE_DISMISSED_EVENT,
              this.onHelpBubbleDismissed_.bind(this));
          this.dismissedEventTracker_.add(
              bubble, HELP_BUBBLE_TIMED_OUT_EVENT,
              this.onHelpBubbleTimedOut_.bind(this));

          bubble.anchorId = anchorId;
          bubble.closeButtonAltText = params.closeButtonAltText;
          bubble.position = params.position;
          bubble.bodyText = params.bodyText;
          bubble.bodyIconName = params.bodyIconName || null;
          bubble.bodyIconAltText = params.bodyIconAltText;
          bubble.forceCloseButton = params.forceCloseButton;
          bubble.titleText = params.titleText || '';
          bubble.progress = params.progress || null;
          bubble.buttons = params.buttons;
          if (params.timeout) {
            bubble.timeoutMs = Number(params.timeout!.microseconds / 1000n);
            assert(bubble.timeoutMs > 0);
          }

          assert(
              !bubble.progress ||
              bubble.progress.total >= bubble.progress.current);
          bubble.show();
          anchor!.focus();
        }

        /**
         * Hides a help bubble anchored to element with id `anchorId` if there
         * is one. Returns true if a bubble was hidden.
         */
        hideHelpBubble(anchorId: string): boolean {
          const bubble = this.getHelpBubbleFor_(anchorId);
          if (!bubble) {
            return false;
          }
          this.dismissedEventTracker_.remove(
              bubble, HELP_BUBBLE_DISMISSED_EVENT);
          bubble.hide();
          bubble.remove();
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
        notifyHelpBubbleAnchorActivated(anchorId: string): boolean {
          if (!this.targetVisibility_.get(anchorId)) {
            return false;
          }
          const nativeId = this.getNativeIdForAnchor_(anchorId);
          assert(nativeId);
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
            anchorId: string, customEvent: string): boolean {
          if (!this.targetVisibility_.get(anchorId)) {
            return false;
          }
          const nativeId = this.getNativeIdForAnchor_(anchorId);
          assert(nativeId);
          this.helpBubbleHandler_.helpBubbleAnchorCustomEvent(
              nativeId, customEvent);
          return true;
        }

        private onTargetVisibilityChanged_(
            target: Element, isVisible: boolean) {
          if (isVisible === this.targetVisibility_.get(target.id)) {
            return;
          }
          this.targetVisibility_.set(target.id, isVisible);
          const hidden = this.hideHelpBubble(target.id);
          const nativeId = this.getNativeIdForAnchor_(target.id);
          assert(nativeId);
          if (hidden) {
            this.helpBubbleHandler_.helpBubbleClosed(
                nativeId, HelpBubbleClosedReason.kPageChanged);
          }
          this.helpBubbleHandler_.helpBubbleAnchorVisibilityChanged(
              nativeId, isVisible);
        }

        /**
         * Observes visibility for element with id `htmlId` to properly send
         * visibility changed events to the associated handler.
         */
        private observeTarget_(htmlId: string) {
          assert(this.helpBubbleTargetObserver_);
          const anchor = this.findAnchorElement_(htmlId);
          assert(anchor, 'Help bubble anchor not found; expected id ' + htmlId);
          this.helpBubbleTargetObserver_.observe(anchor);
        }

        private onShowHelpBubble_(params: HelpBubbleParams): void {
          if (!this.helpBubbleNativeToTargetId_.has(params.nativeIdentifier)) {
            // Identifier not handled by this mixin.
            return;
          }

          const anchorId: string =
              this.helpBubbleNativeToTargetId_.get(params.nativeIdentifier)!;
          this.showHelpBubble(anchorId, params);
        }

        private onToggleHelpBubbleFocusForAccessibility_(nativeId: string) {
          if (!this.helpBubbleNativeToTargetId_.has(nativeId)) {
            // Identifier not handled by this mixin.
            return;
          }

          const anchorId = this.helpBubbleNativeToTargetId_.get(nativeId)!;
          const bubble = this.getHelpBubbleFor_(anchorId);
          if (bubble) {
            const anchor = bubble.getAnchorElement();
            if (anchor) {
              anchor.focus();
            }
          }
        }

        private onHideHelpBubble_(nativeId: string): void {
          if (!this.helpBubbleNativeToTargetId_.has(nativeId)) {
            // Identifier not handled by this mixin.
            return;
          }

          this.hideHelpBubble(this.helpBubbleNativeToTargetId_.get(nativeId)!);
        }

        private findAnchorElement_(anchorId: string): HTMLElement|null {
          return this.shadowRoot!.querySelector<HTMLElement>(`#${anchorId}`);
        }

        private getNativeIdForAnchor_(anchorId: string): string|null {
          for (const [nativeId, htmlId] of this.helpBubbleNativeToTargetId_) {
            if (htmlId === anchorId) {
              return nativeId;
            }
          }
          return null;
        }

        /**
         * Returns a help bubble element anchored to element with HTML id
         * `anchorId`, or null if none.
         */
        private getHelpBubbleFor_(anchorId: string): HelpBubbleElement|null {
          return this.shadowRoot!.querySelector(
              `help-bubble[anchor-id='${anchorId}']`);
        }

        private onHelpBubbleDismissed_(e: HelpBubbleDismissedEvent) {
          const hidden = this.hideHelpBubble(e.detail.anchorId);
          assert(hidden);
          const nativeId = this.getNativeIdForAnchor_(e.detail.anchorId);
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

        private onHelpBubbleTimedOut_(e: HelpBubbleDismissedEvent) {
          const hidden = this.hideHelpBubble(e.detail.anchorId);
          assert(hidden);
          const nativeId = this.getNativeIdForAnchor_(e.detail.anchorId);
          if (nativeId) {
            this.helpBubbleHandler_.helpBubbleClosed(
                nativeId, HelpBubbleClosedReason.kTimedOut);
          }
        }
      }

      return HelpBubbleMixin;
    });

export interface HelpBubbleMixinInterface {
  registerHelpBubbleIdentifier(nativeId: string, htmlId: string): void;
  isHelpBubbleShowing(): boolean;
  isHelpBubbleShowingFor(anchorId: string): boolean;
  showHelpBubble(anchorId: string, params: HelpBubbleParams): void;
  hideHelpBubble(anchorId: string): boolean;
  notifyHelpBubbleAnchorActivated(anchorId: string): boolean;
  notifyHelpBubbleAnchorCustomEvent(anchorId: string, customEvent: string):
      boolean;
}
