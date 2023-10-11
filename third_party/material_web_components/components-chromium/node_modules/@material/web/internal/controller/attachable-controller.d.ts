/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController, ReactiveControllerHost } from 'lit';
/**
 * An element that can be attached to an associated controlling element.
 */
export interface Attachable {
    /**
     * Reflects the value of the `for` attribute, which is the ID of the element's
     * associated control.
     *
     * Use this when the elements's associated control is not its parent.
     *
     * To manually control an element, set its `for` attribute to `""`.
     *
     * @example
     * ```html
     * <div class="container">
     *   <md-attachable for="interactive"></md-attachable>
     *   <button id="interactive">Action</button>
     * </div>
     * ```
     *
     * @example
     * ```html
     * <button class="manually-controlled">
     *   <md-attachable for=""></md-attachable>
     * </button>
     * ```
     */
    htmlFor: string | null;
    /**
     * Gets or sets the element that controls the visibility of the attachable
     * element. It is one of:
     *
     * - The control referenced by the `for` attribute.
     * - The control provided to `element.attach(control)`
     * - The element's parent.
     * - `null` if the element is not controlled.
     */
    control: HTMLElement | null;
    /**
     * Attaches the element to an interactive control.
     *
     * @param control The element that controls the attachable element.
     */
    attach(control: HTMLElement): void;
    /**
     * Detaches the element from its current control.
     */
    detach(): void;
}
/**
 * A key to retrieve an `Attachable` element's `AttachableController` from a
 * global `MutationObserver`.
 */
declare const ATTACHABLE_CONTROLLER: unique symbol;
/**
 * The host of an `AttachableController`. The controller will add itself to
 * the host so it can be retrieved in a global `MutationObserver`.
 */
interface AttachableControllerHost extends ReactiveControllerHost, HTMLElement {
    [ATTACHABLE_CONTROLLER]?: AttachableController;
}
/**
 * A controller that provides an implementation for `Attachable` elements.
 *
 * @example
 * ```ts
 * class MyElement extends LitElement implements Attachable {
 *   get control() { return this.attachableController.control; }
 *
 *   private readonly attachableController = new AttachableController(
 *     this,
 *     (previousControl, newControl) => {
 *       previousControl?.removeEventListener('click', this.handleClick);
 *       newControl?.addEventListener('click', this.handleClick);
 *     }
 *   );
 *
 *   // Implement remaining `Attachable` properties/methods that call the
 *   // controller's properties/methods.
 * }
 * ```
 */
export declare class AttachableController implements ReactiveController, Attachable {
    private readonly host;
    private readonly onControlChange;
    get htmlFor(): string | null;
    set htmlFor(htmlFor: string | null);
    get control(): HTMLElement | null;
    set control(control: HTMLElement | null);
    private currentControl;
    /**
     * Creates a new controller for an `Attachable` element.
     *
     * @param host The `Attachable` element.
     * @param onControlChange A callback with two parameters for the previous and
     *     next control. An `Attachable` element may perform setup or teardown
     *     logic whenever the control changes.
     */
    constructor(host: AttachableControllerHost, onControlChange: (prev: HTMLElement | null, next: HTMLElement | null) => void);
    attach(control: HTMLElement): void;
    detach(): void;
    /** @private */
    hostConnected(): void;
    /** @private */
    hostDisconnected(): void;
    private setCurrentControl;
}
export {};
