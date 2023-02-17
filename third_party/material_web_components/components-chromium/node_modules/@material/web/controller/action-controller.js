/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Enumeration to keep track of the lifecycle of a touch event.
 */
// State transition diagram:
//     +-----------------------------+
//     |                             v
//     |    +------+------ WAITING_FOR_MOUSE_CLICK<----+
//     |    |      |                ^                  |
//     |    V      |                |                  |
// => INACTIVE -> TOUCH_DELAY -> RELEASING          HOLDING
//                 |                                   ^
//                 |                                   |
//                 +-----------------------------------+
var Phase;
(function (Phase) {
    // Initial state of the control, no touch in progress.
    // Transitions:
    //     on touch down: transition to TOUCH_DELAY.
    //     on mouse down: transition to WAITING_FOR_MOUSE_CLICK.
    Phase["INACTIVE"] = "INACTIVE";
    // Touch down has been received, waiting to determine if it's a swipe.
    // Transitions:
    //     on touch up: beginPress(); transition to RELEASING.
    //     on cancel: transition to INACTIVE.
    //     after TOUCH_DELAY_MS: beginPress(); transition to HOLDING.
    Phase["TOUCH_DELAY"] = "TOUCH_DELAY";
    // A touch has been deemed to be a press
    // Transitions:
    //     on pointerup: endPress(); transition to WAITING_FOR_MOUSE_CLICK.
    Phase["HOLDING"] = "HOLDING";
    // The user has released the mouse / touch, but we want to delay calling
    // endPress for a little bit to avoid double clicks.
    // Transitions:
    //    mouse sequence after debounceDelay: endPress(); transition to INACTIVE
    //    when in touch sequence: transitions directly to WAITING_FOR_MOUSE_CLICK
    Phase["RELEASING"] = "RELEASING";
    // The user has touched, but we want to delay endPress until synthetic mouse
    // click event occurs. Stay in this state for a fixed amount of time before
    // giving up and transitioning into rest state.
    // Transitions:
    //     on click: endPress(); transition to INACTIVE.
    //     after WAIT_FOR_MOUSE_CLICK_MS: transition to INACTIVE.
    Phase["WAITING_FOR_MOUSE_CLICK"] = "WAITING_FOR_MOUSE_CLICK";
})(Phase || (Phase = {}));
/**
 * Delay time from touchstart to when element#beginPress is invoked.
 */
export const TOUCH_DELAY_MS = 150;
/**
 * Delay time from beginning to wait for synthetic mouse events till giving up.
 */
export const WAIT_FOR_MOUSE_CLICK_MS = 500;
/**
 * ActionController normalizes user interaction on components and distills it
 * into calling `beginPress` and `endPress` on the component.
 *
 * `beginPress` is a good hook to affect visuals for pressed state, including
 * ripple.
 *
 * `endPress` is a good hook for firing events based on user interaction, and
 * cleaning up the pressed visual state.
 *
 * A component using an ActionController need only implement the ActionElement
 * interface and add the ActionController's event listeners to understand user
 * interaction.
 */
export class ActionController {
    constructor(element) {
        this.element = element;
        this.phase = Phase.INACTIVE;
        this.touchTimer = null;
        this.clickTimer = null;
        this.lastPositionEvent = null;
        this.pressed = false;
        this.checkBoundsAfterContextMenu = false;
        // event listeners
        /**
         * Pointer down event handler.
         */
        this.pointerDown = (e) => {
            if (!this.shouldRespondToEvent(e) || this.phase !== Phase.INACTIVE) {
                return;
            }
            if (this.isTouch(e)) {
                // after a longpress contextmenu event, an extra `pointerdown` can be
                // dispatched to the pressed element. Check that the down is within
                // bounds of the element in this case.
                if (this.checkBoundsAfterContextMenu && !this.inBounds(e)) {
                    return;
                }
                this.checkBoundsAfterContextMenu = false;
                this.lastPositionEvent = e;
                this.setPhase(Phase.TOUCH_DELAY);
                this.touchTimer = setTimeout(() => {
                    this.touchDelayFinished();
                }, TOUCH_DELAY_MS);
            }
            else {
                const leftButtonPressed = e.button === 0;
                if (!leftButtonPressed ||
                    (this.ignoreClicksWithModifiers && this.eventHasModifiers(e))) {
                    return;
                }
                this.setPhase(Phase.WAITING_FOR_MOUSE_CLICK);
                this.beginPress(e);
            }
        };
        /**
         * Pointer up event handler.
         */
        this.pointerUp = (e) => {
            if (!this.isTouch(e) || !this.shouldRespondToEvent(e)) {
                return;
            }
            if (this.phase === Phase.HOLDING) {
                this.waitForClick();
            }
            else if (this.phase === Phase.TOUCH_DELAY) {
                this.setPhase(Phase.RELEASING);
                this.beginPress();
                this.waitForClick();
            }
        };
        /**
         * Click event handler.
         */
        this.click = (e) => {
            if (this.disabled ||
                (this.ignoreClicksWithModifiers && this.eventHasModifiers(e))) {
                return;
            }
            if (this.phase === Phase.WAITING_FOR_MOUSE_CLICK) {
                this.endPress();
                this.setPhase(Phase.INACTIVE);
                return;
            }
            // keyboard synthesized click event
            if (this.phase === Phase.INACTIVE && !this.pressed) {
                this.press();
            }
        };
        /**
         * Pointer leave event handler.
         */
        this.pointerLeave = (e) => {
            // cancel a held press that moves outside the element
            if (this.shouldRespondToEvent(e) && !this.isTouch(e) && this.pressed) {
                this.cancelPress();
            }
        };
        /**
         * Pointer cancel event handler.
         */
        this.pointerCancel = (e) => {
            if (this.shouldRespondToEvent(e)) {
                this.cancelPress();
            }
        };
        /**
         * Contextmenu event handler.
         */
        this.contextMenu = () => {
            if (!this.disabled) {
                this.checkBoundsAfterContextMenu = true;
                this.cancelPress();
            }
        };
        this.element.addController(this);
    }
    get disabled() {
        return this.element.disabled;
    }
    get ignoreClicksWithModifiers() {
        return this.element.ignoreClicksWithModifiers ?? false;
    }
    setPhase(newPhase) {
        this.phase = newPhase;
    }
    /**
     * Calls beginPress and then endPress. Allows us to programmatically click
     * on the element.
     */
    press() {
        this.beginPress(/* positionEvent= */ null);
        this.setPhase(Phase.INACTIVE);
        this.endPress();
    }
    /**
     * Call `beginPress` on element with triggering event, if applicable.
     */
    beginPress(positionEvent = this.lastPositionEvent) {
        this.pressed = true;
        this.element.beginPress({ positionEvent });
    }
    /**
     * Call `endPress` on element, and clean up timers.
     */
    endPress() {
        this.pressed = false;
        this.element.endPress({ cancelled: false });
        this.cleanup();
    }
    cleanup() {
        if (this.touchTimer) {
            clearTimeout(this.touchTimer);
        }
        this.touchTimer = null;
        if (this.clickTimer) {
            clearTimeout(this.clickTimer);
        }
        this.clickTimer = null;
        this.lastPositionEvent = null;
    }
    /**
     * Call `endPress` with cancelled state on element, and cleanup timers.
     */
    cancelPress() {
        this.pressed = false;
        this.cleanup();
        if (this.phase === Phase.TOUCH_DELAY) {
            this.setPhase(Phase.INACTIVE);
        }
        else if (this.phase !== Phase.INACTIVE) {
            this.setPhase(Phase.INACTIVE);
            this.element.endPress({ cancelled: true });
        }
    }
    isTouch(e) {
        return e.pointerType === 'touch';
    }
    touchDelayFinished() {
        if (this.phase !== Phase.TOUCH_DELAY) {
            return;
        }
        this.setPhase(Phase.HOLDING);
        this.beginPress();
    }
    waitForClick() {
        this.setPhase(Phase.WAITING_FOR_MOUSE_CLICK);
        this.clickTimer = setTimeout(() => {
            // If a click event does not occur, clean up the interaction state.
            if (this.phase === Phase.WAITING_FOR_MOUSE_CLICK) {
                this.cancelPress();
            }
        }, WAIT_FOR_MOUSE_CLICK_MS);
    }
    /**
     * Check if event should trigger actions on the element.
     */
    shouldRespondToEvent(e) {
        return !this.disabled && e.isPrimary;
    }
    /**
     * Check if the event is within the bounds of the element.
     *
     * This is only needed for the "stuck" contextmenu longpress on Chrome.
     */
    inBounds(ev) {
        const { top, left, bottom, right } = this.element.getBoundingClientRect();
        const { x, y } = ev;
        return x >= left && x <= right && y >= top && y <= bottom;
    }
    eventHasModifiers(e) {
        return e.altKey || e.ctrlKey || e.shiftKey || e.metaKey;
    }
    /**
     * Cancel interactions if the element is removed from the DOM.
     */
    hostDisconnected() {
        this.cancelPress();
    }
    /**
     * If the element becomes disabled, cancel interactions.
     */
    hostUpdated() {
        if (this.disabled) {
            this.cancelPress();
        }
    }
}
//# sourceMappingURL=action-controller.js.map