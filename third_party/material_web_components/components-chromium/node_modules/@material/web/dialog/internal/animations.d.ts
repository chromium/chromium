/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * A dialog animation's arguments. See `Element.prototype.animate`.
 */
export type DialogAnimationArgs = Parameters<Element['animate']>;
/**
 * A collection of dialog animations. Each element of a dialog may have multiple
 * animations.
 */
export interface DialogAnimation {
    /**
     * Animations for the dialog itself.
     */
    dialog?: DialogAnimationArgs[];
    /**
     * Animations for the scrim backdrop.
     */
    scrim?: DialogAnimationArgs[];
    /**
     * Animations for the container of the dialog.
     */
    container?: DialogAnimationArgs[];
    /**
     * Animations for the headline section.
     */
    headline?: DialogAnimationArgs[];
    /**
     * Animations for the contents section.
     */
    content?: DialogAnimationArgs[];
    /**
     * Animations for the actions section.
     */
    actions?: DialogAnimationArgs[];
}
/**
 * The default dialog open animation.
 */
export declare const DIALOG_DEFAULT_OPEN_ANIMATION: DialogAnimation;
/**
 * The default dialog close animation.
 */
export declare const DIALOG_DEFAULT_CLOSE_ANIMATION: DialogAnimation;
