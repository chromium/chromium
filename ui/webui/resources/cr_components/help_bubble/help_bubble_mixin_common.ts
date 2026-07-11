// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to mixing implementations for supporting help
 * bubbles.
 *
 * See README.md for more information.
 */

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {HELP_BUBBLE_DISMISSED_EVENT, HELP_BUBBLE_TIMED_OUT_EVENT} from './help_bubble.js';
import type {HelpBubbleDismissedEvent, HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleClosedReason} from './help_bubble.mojom-webui.js';
import type {BrowserProxy, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {HelpBubbleController} from './help_bubble_controller.js';
import type {HelpBubbleOptions, Trackable} from './help_bubble_controller.js';

/**
 * Internal implementation for help bubble mixins. See HelpBubbleMixinInterface
 * for documentation for public methods.
 */
export class HelpBubbleMixinCommon {
  private helpBubbleProxy_: BrowserProxy;
  private host_: HTMLElement;
  private isConnected_: boolean = false;

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
  private helpBubbleDismissedEventTracker_: EventTracker = new EventTracker();

  constructor(proxy: BrowserProxy, host: HTMLElement) {
    assert(proxy);
    assert(host);
    this.helpBubbleProxy_ = proxy;
    this.host_ = host;
  }

  private get controllers(): HelpBubbleController[] {
    return Array.from(this.helpBubbleControllerById_.values());
  }

  onConnected() {
    const router = this.helpBubbleProxy_.callbackRouter;
    this.helpBubbleListenerIds_.push(
        router.showHelpBubble.addListener(this.onShowHelpBubble_.bind(this)),
        router.toggleFocusForAccessibility.addListener(
            this.onToggleHelpBubbleFocusForAccessibility_.bind(this)),
        router.hideHelpBubble.addListener(this.onHideHelpBubble_.bind(this)),
        router.externalHelpBubbleUpdated.addListener(
            this.onExternalHelpBubbleUpdated_.bind(this)));

    // When the component is connected, if the target elements were
    // already registered, they should be observed now. Any targets
    // registered from this point forward will observed on registration.
    this.controllers.forEach(
        ctrl => this.observeControllerAnchor_(ctrl, ctrl.getOptions()));
    this.isConnected_ = true;
  }

  onDisconnected() {
    this.isConnected_ = false;
    for (const listenerId of this.helpBubbleListenerIds_) {
      this.helpBubbleProxy_.callbackRouter.removeListener(listenerId);
    }
    this.helpBubbleListenerIds_ = [];
    this.helpBubbleDismissedEventTracker_.removeAll();
    for (const nativeId of this.helpBubbleControllerById_.keys()) {
      this.unregisterHelpBubble(nativeId);
    }
    this.helpBubbleControllerById_.clear();
  }

  registerHelpBubble(
      nativeId: string, trackable: Trackable,
      options: HelpBubbleOptions = {}): HelpBubbleController|null {
    if (this.helpBubbleControllerById_.has(nativeId)) {
      const ctrl = this.helpBubbleControllerById_.get(nativeId);
      if (ctrl && ctrl.isBubbleShowing()) {
        return null;
      }
      this.unregisterHelpBubble(nativeId);
    }
    const controller =
        new HelpBubbleController(nativeId, this.host_.shadowRoot!);
    controller.track(trackable, options);
    this.helpBubbleControllerById_.set(nativeId, controller);
    // This can be called before or after `connectedCallback()`, so if the
    // component isn't connected and the observer set up yet, delay
    // observation until it is.
    if (this.isConnected_) {
      this.observeControllerAnchor_(controller, options);
    }
    return controller;
  }

  unregisterHelpBubble(nativeId: string): void {
    const ctrl = this.helpBubbleControllerById_.get(nativeId);
    if (ctrl && ctrl.hasAnchor()) {
      this.unobserveControllerAnchor_(ctrl);
    }
    this.helpBubbleControllerById_.delete(nativeId);
  }

  private observeControllerAnchor_(
      controller: HelpBubbleController, options: HelpBubbleOptions) {
    const anchor = controller.getAnchor()!;

    TrackedElementManager.getInstance().startTracking(
        anchor, controller.getNativeId(), options,
        (visible: boolean, bounds: RectF) =>
            this.onAnchorVisibilityChanged_(anchor, visible, bounds));
  }

  private unobserveControllerAnchor_(controller: HelpBubbleController) {
    const anchor = controller.getAnchor()!;
    TrackedElementManager.getInstance().stopTracking(anchor);
  }

  isHelpBubbleShowing(): boolean {
    return this.controllers.some(ctrl => ctrl.isBubbleShowing());
  }

  isHelpBubbleShowingForTesting(id: string): boolean {
    const controllers =
        this.controllers.filter(this.filterMatchingIdForTesting_(id));
    return !!controllers[0];
  }

  getHelpBubbleForTesting(id: string): HelpBubbleElement|null {
    const controllers =
        this.controllers.filter(this.filterMatchingIdForTesting_(id));
    return controllers[0] ? controllers[0].getBubble() : null;
  }

  private filterMatchingIdForTesting_(anchorId: string):
      (ctrl: HelpBubbleController) => boolean {
    return ctrl => ctrl.isBubbleShowing() && ctrl.getAnchor() !== null &&
        ctrl.getAnchor()!.id === anchorId;
  }

  getSortedAnchorStatusesForTesting(): Array<[string, boolean]> {
    return this.controllers
        .sort((a, b) => a.getNativeId().localeCompare(b.getNativeId()))
        .map(ctrl => ([ctrl.getNativeId(), ctrl.hasAnchor()]));
  }

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
        otherCtrl =>
            otherCtrl.isBubbleShowing() && otherCtrl.getAnchor() === anchor);
    return !anchorIsUsed;
  }

  showHelpBubble(controller: HelpBubbleController, params: HelpBubbleParams):
      void {
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

  notifyHelpBubbleAnchorActivated(nativeId: string): boolean {
    const ctrl = this.helpBubbleControllerById_.get(nativeId);
    if (!ctrl || !ctrl.isBubbleShowing()) {
      return false;
    }
    const anchor = ctrl.getAnchor()!;
    TrackedElementManager.getInstance().notifyElementActivated(anchor);
    return true;
  }

  notifyHelpBubbleAnchorCustomEvent(nativeId: string, customEvent: string):
      boolean {
    const ctrl = this.helpBubbleControllerById_.get(nativeId);
    if (!ctrl || !ctrl.isBubbleShowing()) {
      return false;
    }
    const anchor = ctrl.getAnchor();
    if (anchor) {
      TrackedElementManager.getInstance().notifyCustomEvent(
          anchor, customEvent);
    }
    return true;
  }

  /**
   * This event is emitted by the TrackedElementManager
   */
  private onAnchorVisibilityChanged_(
      target: HTMLElement, isVisible: boolean, bounds: RectF) {
    const nativeId = target.dataset['nativeId']!;
    assert(nativeId);
    const ctrl = this.helpBubbleControllerById_.get(nativeId);
    if (!isVisible) {
      const hidden = this.hideHelpBubble(nativeId);
      if (hidden) {
        this.helpBubbleProxy_.handler.helpBubbleClosed(
            nativeId, HelpBubbleClosedReason.kPageChanged);
      }
    }
    if (ctrl) {
      ctrl.updateAnchorVisibility(isVisible, bounds);
    }
  }

  /**
   * This event is emitted by the mojo router
   */
  private onShowHelpBubble_(params: HelpBubbleParams): void {
    if (!this.helpBubbleControllerById_.has(params.nativeIdentifier)) {
      // Identifier not handled by this mixin.
      return;
    }
    const ctrl = this.helpBubbleControllerById_.get(params.nativeIdentifier)!;
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
        this.helpBubbleProxy_.handler.helpBubbleButtonPressed(
            nativeId, e.detail.buttonIndex!);
      } else {
        this.helpBubbleProxy_.handler.helpBubbleClosed(
            nativeId, HelpBubbleClosedReason.kDismissedByUser);
      }
    }
  }

  /**
   * This event is emitted by the help-bubble component
   */
  private onHelpBubbleTimedOut_(e: HelpBubbleDismissedEvent) {
    const nativeId = e.detail.nativeId;
    assert(nativeId);
    const hidden = this.hideHelpBubble(nativeId);
    assert(hidden);
    if (nativeId) {
      this.helpBubbleProxy_.handler.helpBubbleClosed(
          nativeId, HelpBubbleClosedReason.kTimedOut);
    }
  }
}
