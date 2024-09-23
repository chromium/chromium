/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ReactiveController, ReactiveControllerHost } from 'lit';
import { StyleInfo } from 'lit/directives/style-map.js';
/**
 * Declare popoverAPI functions and properties. See
 * https://developer.mozilla.org/en-US/docs/Web/API/Popover_API
 * Without this, closure will rename these functions. Can remove once these
 * functions make it into the typescript lib.
 */
declare global {
    interface HTMLElement {
        showPopover(): void;
        hidePopover(): void;
        togglePopover(force: boolean): void;
        popover: string | null;
    }
}
/**
 * An enum of supported Menu corners
 */
export declare const Corner: {
    readonly END_START: "end-start";
    readonly END_END: "end-end";
    readonly START_START: "start-start";
    readonly START_END: "start-end";
};
/**
 * A corner of a box in the standard logical property style of <block>_<inline>
 */
export type Corner = (typeof Corner)[keyof typeof Corner];
/**
 * An interface that provides a method to customize the rect from which to
 * calculate the anchor positioning. Useful for when you want a surface to
 * anchor to an element in your shadow DOM rather than the host element.
 */
export interface SurfacePositionTarget extends HTMLElement {
    getSurfacePositionClientRect?: () => DOMRect;
}
/**
 * The configurable options for the surface position controller.
 */
export interface SurfacePositionControllerProperties {
    /**
     * Disable the `flip` behavior on the block axis of the surface's corner
     */
    disableBlockFlip: boolean;
    /**
     * Disable the `flip` behavior on the inline axis of the surface's corner
     */
    disableInlineFlip: boolean;
    /**
     * The corner of the anchor to align the surface's position.
     */
    anchorCorner: Corner;
    /**
     * The corner of the surface to align to the given anchor corner.
     */
    surfaceCorner: Corner;
    /**
     * The HTMLElement reference of the surface to be positioned.
     */
    surfaceEl: SurfacePositionTarget | null;
    /**
     * The HTMLElement reference of the anchor to align to.
     */
    anchorEl: SurfacePositionTarget | null;
    /**
     * Whether the positioning algorithim should calculate relative to the parent
     * of the anchor element (absolute) or relative to the window (fixed).
     *
     * Examples for `position = 'fixed'`:
     *
     * - If there is no `position:relative` in the given parent tree and the
     *   surface is `position:absolute`
     * - If the surface is `position:fixed`
     * - If the surface is in the "top layer"
     * - The anchor and the surface do not share a common `position:relative`
     *   ancestor
     */
    positioning: 'absolute' | 'fixed' | 'document';
    /**
     * Whether or not the surface should be "open" and visible
     */
    isOpen: boolean;
    /**
     * The number of pixels in which to offset from the inline axis relative to
     * logical property.
     *
     * Positive is right in LTR and left in RTL.
     */
    xOffset: number;
    /**
     * The number of pixes in which to offset the block axis.
     *
     * Positive is down and negative is up.
     */
    yOffset: number;
    /**
     * The strategy to follow when repositioning the menu to stay inside the
     * viewport. "move" will simply move the surface to stay in the viewport.
     * "resize" will attempt to resize the surface.
     *
     * Both strategies will still attempt to flip the anchor and surface corners.
     */
    repositionStrategy: 'move' | 'resize';
    /**
     * A function to call after the surface has been positioned.
     */
    onOpen: () => void;
    /**
     * A function to call before the surface should be closed. (A good time to
     * perform animations while the surface is still visible)
     */
    beforeClose: () => Promise<void>;
    /**
     * A function to call after the surface has been closed.
     */
    onClose: () => void;
}
/**
 * Given a surface, an anchor, corners, and some options, this surface will
 * calculate the position of a surface to align the two given corners and keep
 * the surface inside the window viewport. It also provides a StyleInfo map that
 * can be applied to the surface to handle visiblility and position.
 */
export declare class SurfacePositionController implements ReactiveController {
    private readonly host;
    private readonly getProperties;
    private surfaceStylesInternal;
    private lastValues;
    /**
     * @param host The host to connect the controller to.
     * @param getProperties A function that returns the properties for the
     * controller.
     */
    constructor(host: ReactiveControllerHost, getProperties: () => SurfacePositionControllerProperties);
    /**
     * The StyleInfo map to apply to the surface via Lit's stylemap
     */
    get surfaceStyles(): StyleInfo;
    /**
     * Calculates the surface's new position required so that the surface's
     * `surfaceCorner` aligns to the anchor's `anchorCorner` while keeping the
     * surface inside the window viewport. This positioning also respects RTL by
     * checking `getComputedStyle()` on the surface element.
     */
    position(): Promise<void>;
    /**
     * Calculates the css property, the inset, and the out of bounds correction
     * for the surface in the block direction.
     */
    private calculateBlock;
    /**
     * Calculates the css property, the inset, and the out of bounds correction
     * for the surface in the inline direction.
     */
    private calculateInline;
    hostUpdate(): void;
    hostUpdated(): void;
    /**
     * Checks whether the properties passed into the controller have changed since
     * the last positioning. If so, it will reposition if the surface is open or
     * close it if the surface should close.
     */
    private onUpdate;
    /**
     * Hides the surface.
     */
    private close;
}
