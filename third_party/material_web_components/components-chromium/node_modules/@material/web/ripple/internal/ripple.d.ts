/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues } from 'lit';
import { Attachable } from '../../internal/controller/attachable-controller.js';
/**
 * A ripple component.
 */
export declare class Ripple extends LitElement implements Attachable {
    /**
     * Disables the ripple.
     */
    disabled: boolean;
    get htmlFor(): string | null;
    set htmlFor(htmlFor: string | null);
    get control(): HTMLElement | null;
    set control(control: HTMLElement | null);
    private hovered;
    private pressed;
    private readonly mdRoot;
    private rippleSize;
    private rippleScale;
    private initialSize;
    private growAnimation?;
    private state;
    private rippleStartEvent?;
    private checkBoundsAfterContextMenu;
    private readonly attachableController;
    attach(control: HTMLElement): void;
    detach(): void;
    connectedCallback(): void;
    protected render(): import("lit-html").TemplateResult<1>;
    protected update(changedProps: PropertyValues<Ripple>): void;
    /**
     * TODO(b/269799771): make private
     * @private only public for slider
     */
    handlePointerenter(event: PointerEvent): void;
    /**
     * TODO(b/269799771): make private
     * @private only public for slider
     */
    handlePointerleave(event: PointerEvent): void;
    private handlePointerup;
    private handlePointerdown;
    private handleClick;
    private handlePointercancel;
    private handleContextmenu;
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
    /** @private */
    handleEvent(event: Event): Promise<void>;
    private onControlChange;
}
