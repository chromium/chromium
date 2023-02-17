/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCMenuDimensions, MDCMenuDistance, MDCMenuPoint } from './types.js';
/**
 * Defines the shape of the adapter expected by the foundation.
 * Implement this adapter for your framework of choice to delegate updates to
 * the component in your framework of choice. See architecture documentation
 * for more details.
 * https://github.com/material-components/material-components-web/blob/master/docs/code/architecture.md
 */
export interface MDCMenuSurfaceAdapter {
    addClass(className: string): void;
    removeClass(className: string): void;
    hasClass(className: string): boolean;
    hasAnchor(): boolean;
    isElementInContainer(el: Element): boolean;
    isFocused(): boolean;
    isRtl(): boolean;
    getInnerDimensions(): MDCMenuDimensions;
    getAnchorDimensions(): DOMRect | null;
    getWindowDimensions(): MDCMenuDimensions;
    getBodyDimensions(): MDCMenuDimensions;
    getWindowScroll(): MDCMenuPoint;
    setPosition(position: Partial<MDCMenuDistance>): void;
    setMaxHeight(height: string): void;
    setTransformOrigin(origin: string): void;
    getOwnerDocument?(): Document;
    /** Saves the element that was focused before the menu surface was opened. */
    saveFocus(): void;
    /**
     * Restores focus to the element that was focused before the menu surface was
     * opened.
     */
    restoreFocus(): void;
    /** Emits an event when the menu surface is closed. */
    notifyClose(): void;
    /** Emits an event when the menu surface is closing. */
    notifyClosing(): void;
    /** Emits an event when the menu surface is opened. */
    notifyOpen(): void;
    /** Emits an event when the menu surface is opening. */
    notifyOpening(): void;
}
