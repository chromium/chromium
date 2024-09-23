/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../divider/divider.js';
import { LitElement } from 'lit';
import { DialogAnimation } from './animations.js';
declare const dialogBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * A dialog component.
 *
 * @fires open {Event} Dispatched when the dialog is opening before any animations.
 * @fires opened {Event} Dispatched when the dialog has opened after any animations.
 * @fires close {Event} Dispatched when the dialog is closing before any animations.
 * @fires closed {Event} Dispatched when the dialog has closed after any animations.
 * @fires cancel {Event} Dispatched when the dialog has been canceled by clicking
 * on the scrim or pressing Escape.
 */
export declare class Dialog extends dialogBaseClass {
    /**
     * Opens the dialog when set to `true` and closes it when set to `false`.
     */
    get open(): boolean;
    set open(open: boolean);
    /**
     * Skips the opening and closing animations.
     */
    quick: boolean;
    /**
     * Gets or sets the dialog's return value, usually to indicate which button
     * a user pressed to close it.
     *
     * https://developer.mozilla.org/en-US/docs/Web/API/HTMLDialogElement/returnValue
     */
    returnValue: string;
    /**
     * The type of dialog for accessibility. Set this to `alert` to announce a
     * dialog as an alert dialog.
     */
    type?: 'alert';
    /**
     * Disables focus trapping, which by default keeps keyboard Tab navigation
     * within the dialog.
     *
     * When disabled, after focusing the last element of a dialog, pressing Tab
     * again will release focus from the window back to the browser (such as the
     * URL bar).
     *
     * Focus trapping is recommended for accessibility, and should not typically
     * be disabled. Only turn this off if the use case of a dialog is more
     * accessible without focus trapping.
     */
    noFocusTrap: boolean;
    /**
     * Gets the opening animation for a dialog. Set to a new function to customize
     * the animation.
     */
    getOpenAnimation: () => DialogAnimation;
    /**
     * Gets the closing animation for a dialog. Set to a new function to customize
     * the animation.
     */
    getCloseAnimation: () => DialogAnimation;
    private isOpen;
    private isOpening;
    private isConnectedPromiseResolve;
    private isConnectedPromise;
    private readonly dialog;
    private readonly scrim;
    private readonly container;
    private readonly headline;
    private readonly content;
    private readonly actions;
    private isAtScrollTop;
    private isAtScrollBottom;
    private readonly scroller;
    private readonly topAnchor;
    private readonly bottomAnchor;
    private readonly firstFocusTrap;
    private nextClickIsFromContent;
    private intersectionObserver?;
    private hasHeadline;
    private hasActions;
    private hasIcon;
    private cancelAnimations?;
    private escapePressedWithoutCancel;
    private readonly treewalker;
    constructor();
    /**
     * Opens the dialog and fires a cancelable `open` event. After a dialog's
     * animation, an `opened` event is fired.
     *
     * Add an `autofocus` attribute to a child of the dialog that should
     * receive focus after opening.
     *
     * @return A Promise that resolves after the animation is finished and the
     *     `opened` event was fired.
     */
    show(): Promise<void>;
    /**
     * Closes the dialog and fires a cancelable `close` event. After a dialog's
     * animation, a `closed` event is fired.
     *
     * @param returnValue A return value usually indicating which button was used
     *     to close a dialog. If a dialog is canceled by clicking the scrim or
     *     pressing Escape, it will not change the return value after closing.
     * @return A Promise that resolves after the animation is finished and the
     *     `closed` event was fired.
     */
    close(returnValue?: string): Promise<void>;
    connectedCallback(): void;
    disconnectedCallback(): void;
    protected render(): import("lit-html").TemplateResult<1>;
    protected firstUpdated(): void;
    private handleDialogClick;
    private handleContentClick;
    private handleSubmit;
    private handleCancel;
    private handleClose;
    private handleKeydown;
    private animateDialog;
    private handleHeadlineChange;
    private handleActionsChange;
    private handleIconChange;
    private handleAnchorIntersection;
    private getIsConnectedPromise;
    private handleFocusTrapFocus;
    private getFirstAndLastFocusableChildren;
}
export {};
