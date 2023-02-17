/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController, ReactiveControllerHost } from 'lit';
/**
 * Delay time from touchstart to when element#beginPress is invoked.
 */
export declare const TOUCH_DELAY_MS = 150;
/**
 * Delay time from beginning to wait for synthetic mouse events till giving up.
 */
export declare const WAIT_FOR_MOUSE_CLICK_MS = 500;
/**
 * Interface for argument to beginPress.
 */
export interface BeginPressConfig {
    /**
     * Event that was recorded at the start of the interaction.
     * `null` if the press happened via keyboard.
     */
    positionEvent: Event | null;
}
/**
 * Interface for argument to endPress.
 */
export interface EndPressConfig {
    /**
     * `true` if the press was cancelled.
     */
    cancelled: boolean;
    /**
     * Data object to pass along to clients in the `action` event, if relevant.
     */
    actionData?: {};
}
/**
 * The necessary interface for using an ActionController
 */
export interface ActionControllerHost extends ReactiveControllerHost, HTMLElement {
    disabled: boolean;
    /**
     * Determines if pointerdown or click events containing modifier keys should
     * be ignored.
     */
    ignoreClicksWithModifiers?: boolean;
    /**
     * Called when a user interaction is determined to be a press.
     */
    beginPress(config: BeginPressConfig): void;
    /**
     * Called when a press ends or is cancelled.
     */
    endPress(config: EndPressConfig): void;
}
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
export declare class ActionController implements ReactiveController {
    private readonly element;
    constructor(element: ActionControllerHost);
    private get disabled();
    private get ignoreClicksWithModifiers();
    private phase;
    private touchTimer;
    private clickTimer;
    private lastPositionEvent;
    private pressed;
    private checkBoundsAfterContextMenu;
    private setPhase;
    /**
     * Calls beginPress and then endPress. Allows us to programmatically click
     * on the element.
     */
    private press;
    /**
     * Call `beginPress` on element with triggering event, if applicable.
     */
    private beginPress;
    /**
     * Call `endPress` on element, and clean up timers.
     */
    private endPress;
    private cleanup;
    /**
     * Call `endPress` with cancelled state on element, and cleanup timers.
     */
    private cancelPress;
    private isTouch;
    private touchDelayFinished;
    private waitForClick;
    /**
     * Check if event should trigger actions on the element.
     */
    private shouldRespondToEvent;
    /**
     * Check if the event is within the bounds of the element.
     *
     * This is only needed for the "stuck" contextmenu longpress on Chrome.
     */
    private inBounds;
    private eventHasModifiers;
    /**
     * Cancel interactions if the element is removed from the DOM.
     */
    hostDisconnected(): void;
    /**
     * If the element becomes disabled, cancel interactions.
     */
    hostUpdated(): void;
    /**
     * Pointer down event handler.
     */
    pointerDown: (e: PointerEvent) => void;
    /**
     * Pointer up event handler.
     */
    pointerUp: (e: PointerEvent) => void;
    /**
     * Click event handler.
     */
    click: (e: MouseEvent) => void;
    /**
     * Pointer leave event handler.
     */
    pointerLeave: (e: PointerEvent) => void;
    /**
     * Pointer cancel event handler.
     */
    pointerCancel: (e: PointerEvent) => void;
    /**
     * Contextmenu event handler.
     */
    contextMenu: () => void;
}
