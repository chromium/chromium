/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
declare const cssClasses: {
    ANCHOR: string;
    ANIMATING_CLOSED: string;
    ANIMATING_OPEN: string;
    FIXED: string;
    IS_OPEN_BELOW: string;
    OPEN: string;
    ROOT: string;
};
declare const strings: {
    CLOSED_EVENT: string;
    CLOSING_EVENT: string;
    OPENED_EVENT: string;
    OPENING_EVENT: string;
    FOCUSABLE_ELEMENTS: string;
};
declare const numbers: {
    /** Total duration of menu-surface open animation. */
    TRANSITION_OPEN_DURATION: number;
    /** Total duration of menu-surface close animation. */
    TRANSITION_CLOSE_DURATION: number;
    /**
     * Margin left to the edge of the viewport when menu-surface is at maximum
     * possible height. Also used as a viewport margin.
     */
    MARGIN_TO_EDGE: number;
    /**
     * Ratio of anchor width to menu-surface width for switching from corner
     * positioning to center positioning.
     */
    ANCHOR_TO_MENU_SURFACE_WIDTH_RATIO: number;
    /**
     * Amount of time to wait before restoring focus when closing the menu
     * surface. This is important because if a touch event triggered the menu
     * close, and the subsequent mouse event occurs after focus is restored, then
     * the restored focus would be lost.
     */
    TOUCH_EVENT_WAIT_MS: number;
};
/**
 * Enum for bits in the {@see Corner) bitmap.
 */
declare enum CornerBit {
    BOTTOM = 1,
    CENTER = 2,
    RIGHT = 4,
    FLIP_RTL = 8
}
/**
 * Enum for representing an element corner for positioning the menu-surface.
 *
 * The START constants map to LEFT if element directionality is left
 * to right and RIGHT if the directionality is right to left.
 * Likewise END maps to RIGHT or LEFT depending on the directionality.
 */
declare enum Corner {
    TOP_LEFT = 0,
    TOP_RIGHT = 4,
    BOTTOM_LEFT = 1,
    BOTTOM_RIGHT = 5,
    TOP_START = 8,
    TOP_END = 12,
    BOTTOM_START = 9,
    BOTTOM_END = 13
}
export { cssClasses, strings, numbers, CornerBit, Corner };
