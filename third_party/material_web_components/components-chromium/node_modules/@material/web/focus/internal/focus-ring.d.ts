/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, PropertyValues } from 'lit';
import { Attachable } from '../../internal/controller/attachable-controller.js';
/**
 * A focus ring component.
 *
 * @fires visibility-changed {Event} Fired whenever `visible` changes.
 */
export declare class FocusRing extends LitElement implements Attachable {
    /**
     * Makes the focus ring visible.
     */
    visible: boolean;
    /**
     * Makes the focus ring animate inwards instead of outwards.
     */
    inward: boolean;
    get htmlFor(): string | null;
    set htmlFor(htmlFor: string | null);
    get control(): HTMLElement | null;
    set control(control: HTMLElement | null);
    private readonly attachableController;
    attach(control: HTMLElement): void;
    detach(): void;
    connectedCallback(): void;
    /** @private */
    handleEvent(event: FocusRingEvent): void;
    private onControlChange;
    update(changed: PropertyValues<FocusRing>): void;
}
declare const HANDLED_BY_FOCUS_RING: unique symbol;
interface FocusRingEvent extends Event {
    [HANDLED_BY_FOCUS_RING]: true;
}
export {};
