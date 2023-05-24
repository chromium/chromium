/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues } from 'lit';
/**
 * A ripple component.
 */
export declare class Ripple extends LitElement {
    /**
     * Sets the ripple to be an unbounded circle.
     */
    unbounded: boolean;
    /**
     * Disables the ripple.
     */
    disabled: boolean;
    private hovered;
    private focused;
    private pressed;
    private readonly mdRoot;
    private rippleSize;
    private rippleScale;
    private initialSize;
    private growAnimation?;
    private state;
    private rippleStartEvent?;
    private checkBoundsAfterContextMenu;
    handlePointerenter(event: PointerEvent): void;
    handlePointerleave(event: PointerEvent): void;
    handleFocusin(): void;
    handleFocusout(): void;
    handlePointerup(event: PointerEvent): void;
    handlePointerdown(event: PointerEvent): Promise<void>;
    handleClick(): void;
    handlePointercancel(event: PointerEvent): void;
    handleContextmenu(): void;
    protected render(): import("lit-html").TemplateResult<1>;
    protected update(changedProps: PropertyValues<this>): void;
    private getDimensions;
    private determineRippleSize;
    private getNormalizedPointerEventCoords;
    private getTranslationCoordinates;
    private startPressAnimation;
    private endPressAnimation;
    /**
     * Returns `true` if
     *  - the ripple element is enabled
     *  - the pointer is primary for the input type
     *  - the pointer is the pointer that started the interaction, or will start
     * the interaction
     *  - the pointer is a touch, or the pointer state has the primary button
     * held, or the pointer is hovering
     */
    private shouldReactToEvent;
    /**
     * Check if the event is within the bounds of the element.
     *
     * This is only needed for the "stuck" contextmenu longpress on Chrome.
     */
    private inBounds;
    private isTouch;
}
