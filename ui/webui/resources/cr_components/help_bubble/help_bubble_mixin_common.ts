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
import type {TrackedElementVisibilityUpdate} from '//resources/js/tracked_element/tracked_element_manager.js';
import type {TrackedElementIdentifier} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

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
      options: HelpBubbleOptions = {}): boolean {
    if (this.helpBubbleControllerById_.has(nativeId)) {
      const ctrl = this.helpBubbleControllerById_.get(nativeId);
      if (ctrl && ctrl.isBubbleShowing()) {
        return false;
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
    return true;
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
        this.onAnchorVisibilityChanged_.bind(this));
  }

  private unobserveControllerAnchor_(controller: HelpBubbleController) {
    const anchor = controller.getAnchor()!;
    TrackedElementManager.getInstance().stopTracking(anchor);
  }

  isHelpBubbleShowing(): boolean {
    return this.controllers.some(ctrl => ctrl.isBubbleShowing());
  }

  isHelpBubbleShowingForTesting(target: string|HTMLElement): boolean {
    const controllers =
        this.controllers.filter(this.filterControllersForTesting_(target));
    return !!controllers[0];
  }

  getHelpBubbleForTesting(target: string|HTMLElement): HelpBubbleElement|null {
    const controllers =
        this.controllers.filter(this.filterControllersForTesting_(target));
    return controllers[0] ? controllers[0].getBubble() : null;
  }

  private static matchesAnchor_(
      ctrl: HelpBubbleController, target: string|HTMLElement): boolean {
    const anchor = ctrl.getAnchor();
    if (!anchor) {
      return false;
    }
    if (target instanceof HTMLElement) {
      return anchor === target;
    }
    return anchor.id === target || ctrl.getNativeId() === target;
  }

  private filterControllersForTesting_(target: string|HTMLElement):
      (ctrl: HelpBubbleController) => boolean {
    return ctrl => ctrl.isBubbleShowing() &&
        HelpBubbleMixinCommon.matchesAnchor_(ctrl, target);
  }

  getSortedAnchorStatusesForTesting(): Array<[string, boolean]> {
    return this.controllers
        .sort((a, b) => a.getNativeId().localeCompare(b.getNativeId()))
        .map(ctrl => ([ctrl.getNativeId(), ctrl.hasAnchor()]));
  }

  canShowHelpBubble(anchorId: string): boolean {
    const controller = this.helpBubbleControllerById_.get(anchorId);
    if (!controller) {
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

  showHelpBubble(params: HelpBubbleParams): void {
    assert(
        this.canShowHelpBubble(params.id.nativeIdentifier),
        'Can\'t show help bubble');
    const controller =
        this.helpBubbleControllerById_.get(params.id.nativeIdentifier)!;
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
  private onAnchorVisibilityChanged_(update: TrackedElementVisibilityUpdate) {
    const nativeIdentifier = update.element.dataset['nativeId']!;
    const secondaryIdentifier = update.element.dataset['secondaryId']!;
    const ctrl = this.helpBubbleControllerById_.get(nativeIdentifier);
    if (!ctrl || ctrl.getAnchor() !== update.element) {
      // If we've signed up for broader notifications than usual, we might get
      // one that doesn't apply to our specific anchor. Ignore it.
      return;
    }
    if (!update.visible) {
      const hidden = this.hideHelpBubble(nativeIdentifier);
      if (hidden) {
        this.helpBubbleProxy_.handler.helpBubbleClosed(
            {nativeIdentifier, secondaryIdentifier},
            HelpBubbleClosedReason.kPageChanged);
      }
    }
    if (ctrl) {
      ctrl.updateAnchorVisibility(update.visible, update.bounds);
    }
  }

  /**
   * This event is emitted by the mojo router
   */
  private onShowHelpBubble_(params: HelpBubbleParams): void {
    if (!this.helpBubbleControllerById_.has(params.id.nativeIdentifier)) {
      // Identifier not handled by this mixin.
      return;
    }
    this.showHelpBubble(params);
  }

  /**
   * This event is emitted by the mojo router
   */
  private onToggleHelpBubbleFocusForAccessibility_(
      id: TrackedElementIdentifier) {
    if (!this.helpBubbleControllerById_.has(id.nativeIdentifier)) {
      // Identifier not handled by this mixin.
      return;
    }

    const ctrl = this.helpBubbleControllerById_.get(id.nativeIdentifier)!;
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
  private onHideHelpBubble_(id: TrackedElementIdentifier): void {
    // This may be called with nativeId not handled by this mixin
    // Ignore return value to silently fail
    this.hideHelpBubble(id.nativeIdentifier);
  }

  /**
   * This event is emitted by the mojo router.
   */
  private onExternalHelpBubbleUpdated_(
      id: TrackedElementIdentifier, shown: boolean) {
    if (!this.helpBubbleControllerById_.has(id.nativeIdentifier)) {
      // Identifier not handled by this mixin.
      return;
    }

    // Get the associated bubble and update status
    const ctrl = this.helpBubbleControllerById_.get(id.nativeIdentifier)!;
    ctrl.updateExternalShowingStatus(shown);
  }

  /**
   * This event is emitted by the help-bubble component
   */
  private onHelpBubbleDismissed_(e: HelpBubbleDismissedEvent) {
    const nativeIdentifier = e.detail.nativeId;
    const secondaryIdentifier = e.detail.secondaryId;
    assert(nativeIdentifier);
    assert(secondaryIdentifier);
    const hidden = this.hideHelpBubble(nativeIdentifier);
    assert(hidden);
    if (e.detail.fromActionButton) {
      this.helpBubbleProxy_.handler.helpBubbleButtonPressed(
          {nativeIdentifier, secondaryIdentifier}, e.detail.buttonIndex!);
    } else {
      this.helpBubbleProxy_.handler.helpBubbleClosed(
          {nativeIdentifier, secondaryIdentifier},
          HelpBubbleClosedReason.kDismissedByUser);
    }
  }

  /**
   * This event is emitted by the help-bubble component
   */
  private onHelpBubbleTimedOut_(e: HelpBubbleDismissedEvent) {
    const nativeIdentifier = e.detail.nativeId;
    const secondaryIdentifier = e.detail.secondaryId;
    assert(nativeIdentifier);
    assert(secondaryIdentifier);
    const hidden = this.hideHelpBubble(nativeIdentifier);
    assert(hidden);
    this.helpBubbleProxy_.handler.helpBubbleClosed(
        {nativeIdentifier, secondaryIdentifier},
        HelpBubbleClosedReason.kTimedOut);
  }
}
