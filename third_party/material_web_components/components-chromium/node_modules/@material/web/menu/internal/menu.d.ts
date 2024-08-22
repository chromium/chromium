/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import '../../focus/md-focus-ring.js';
import { LitElement, PropertyValues } from 'lit';
import { MenuItem } from './controllers/menuItemController.js';
import { FocusState } from './controllers/shared.js';
import { Corner, SurfacePositionTarget } from './controllers/surfacePositionController.js';
import { TypeaheadController } from './controllers/typeaheadController.js';
export { Corner } from './controllers/surfacePositionController.js';
/**
 * The default value for the typeahead buffer time in Milliseconds.
 */
export declare const DEFAULT_TYPEAHEAD_BUFFER_TIME = 200;
/**
 * @fires opening {Event} Fired before the opening animation begins
 * @fires opened {Event} Fired once the menu is open, after any animations
 * @fires closing {Event} Fired before the closing animation begins
 * @fires closed {Event} Fired once the menu is closed, after any animations
 */
export declare abstract class Menu extends LitElement {
    private readonly surfaceEl;
    private readonly slotEl;
    /**
     * The ID of the element in the same root node in which the menu should align
     * to. Overrides setting `anchorElement = elementReference`.
     *
     * __NOTE__: anchor or anchorElement must either be an HTMLElement or resolve
     * to an HTMLElement in order for menu to open.
     */
    anchor: string;
    /**
     * Whether the positioning algorithm should calculate relative to the parent
     * of the anchor element (`absolute`), relative to the window (`fixed`), or
     * relative to the document (`document`). `popover` will use the popover API
     * to render the menu in the top-layer. If your browser does not support the
     * popover API, it will fall back to `fixed`.
     *
     * __Examples for `position = 'fixed'`:__
     *
     * - If there is no `position:relative` in the given parent tree and the
     *   surface is `position:absolute`
     * - If the surface is `position:fixed`
     * - If the surface is in the "top layer"
     * - The anchor and the surface do not share a common `position:relative`
     *   ancestor
     *
     * When using `positioning=fixed`, in most cases, the menu should position
     * itself above most other `position:absolute` or `position:fixed` elements
     * when placed inside of them. e.g. using a menu inside of an `md-dialog`.
     *
     * __NOTE__: Fixed menus will not scroll with the page and will be fixed to
     * the window instead.
     *
     * __Examples for `position = 'document'`:__
     *
     * - There is no parent that creates a relative positioning context e.g.
     *   `position: relative`, `position: absolute`, `transform: translate(x, y)`,
     *   etc.
     * - You put the effort into hoisting the menu to the top of the DOM like the
     *   end of the `<body>` to render over everything or in a top-layer.
     * - You are reusing a single `md-menu` element that dynamically renders
     *   content.
     *
     * __Examples for `position = 'popover'`:__
     *
     * - Your browser supports `popover`.
     * - Most cases. Once popover is in browsers, this will become the default.
     */
    positioning: 'absolute' | 'fixed' | 'document' | 'popover';
    /**
     * Skips the opening and closing animations.
     */
    quick: boolean;
    /**
     * Displays overflow content like a submenu. Not required in most cases when
     * using `positioning="popover"`.
     *
     * __NOTE__: This may cause adverse effects if you set
     * `md-menu {max-height:...}`
     * and have items overflowing items in the "y" direction.
     */
    hasOverflow: boolean;
    /**
     * Opens the menu and makes it visible. Alternative to the `.show()` and
     * `.close()` methods
     */
    open: boolean;
    /**
     * Offsets the menu's inline alignment from the anchor by the given number in
     * pixels. This value is direction aware and will follow the LTR / RTL
     * direction.
     *
     * e.g. LTR: positive -> right, negative -> left
     *      RTL: positive -> left, negative -> right
     */
    xOffset: number;
    /**
     * Offsets the menu's block alignment from the anchor by the given number in
     * pixels.
     *
     * e.g. positive -> down, negative -> up
     */
    yOffset: number;
    /**
     * Disable the `flip` behavior that usually happens on the horizontal axis
     * when the surface would render outside the viewport.
     */
    noHorizontalFlip: boolean;
    /**
     * Disable the `flip` behavior that usually happens on the vertical axis when
     * the surface would render outside the viewport.
     */
    noVerticalFlip: boolean;
    /**
     * The max time between the keystrokes of the typeahead menu behavior before
     * it clears the typeahead buffer.
     */
    typeaheadDelay: number;
    /**
     * The corner of the anchor which to align the menu in the standard logical
     * property style of <block>-<inline> e.g. `'end-start'`.
     *
     * NOTE: This value may not be respected by the menu positioning algorithm
     * if the menu would render outisde the viewport.
     * Use `no-horizontal-flip` or `no-vertical-flip` to force the usage of the value
     */
    anchorCorner: Corner;
    /**
     * The corner of the menu which to align the anchor in the standard logical
     * property style of <block>-<inline> e.g. `'start-start'`.
     *
     * NOTE: This value may not be respected by the menu positioning algorithm
     * if the menu would render outisde the viewport.
     * Use `no-horizontal-flip` or `no-vertical-flip` to force the usage of the value
     */
    menuCorner: Corner;
    /**
     * Keeps the user clicks outside the menu.
     *
     * NOTE: clicking outside may still cause focusout to close the menu so see
     * `stayOpenOnFocusout`.
     */
    stayOpenOnOutsideClick: boolean;
    /**
     * Keeps the menu open when focus leaves the menu's composed subtree.
     *
     * NOTE: Focusout behavior will stop propagation of the focusout event. Set
     * this property to true to opt-out of menu's focusout handling altogether.
     */
    stayOpenOnFocusout: boolean;
    /**
     * After closing, does not restore focus to the last focused element before
     * the menu was opened.
     */
    skipRestoreFocus: boolean;
    /**
     * The element that should be focused by default once opened.
     *
     * NOTE: When setting default focus to 'LIST_ROOT', remember to change
     * `tabindex` to `0` and change md-menu's display to something other than
     * `display: contents` when necessary.
     */
    defaultFocus: FocusState;
    /**
     * Turns off navigation wrapping. By default, navigating past the end of the
     * menu items will wrap focus back to the beginning and vice versa. Use this
     * for ARIA patterns that do not wrap focus, like combobox.
     */
    noNavigationWrap: boolean;
    protected slotItems: HTMLElement[];
    private typeaheadActive;
    /**
     * Whether or not the current menu is a submenu and should not handle specific
     * navigation keys.
     *
     * @export
     */
    isSubmenu: boolean;
    /**
     * The event path of the last window pointerdown event.
     */
    private pointerPath;
    /**
     * Whether or not the menu is repositoining due to window / document resize
     */
    private isRepositioning;
    private readonly openCloseAnimationSignal;
    private readonly listController;
    /**
     * Whether the menu is animating upwards or downwards when opening. This is
     * helpful for calculating some animation calculations.
     */
    private get openDirection();
    /**
     * The element that was focused before the menu opened.
     */
    private lastFocusedElement;
    /**
     * Handles typeahead navigation through the menu.
     */
    typeaheadController: TypeaheadController;
    private currentAnchorElement;
    /**
     * The element which the menu should align to. If `anchor` is set to a
     * non-empty idref string, then `anchorEl` will resolve to the element with
     * the given id in the same root node. Otherwise, `null`.
     */
    get anchorElement(): (HTMLElement & Partial<SurfacePositionTarget>) | null;
    set anchorElement(element: (HTMLElement & Partial<SurfacePositionTarget>) | null);
    private readonly internals;
    constructor();
    /**
     * Handles positioning the surface and aligning it to the anchor as well as
     * keeping it in the viewport.
     */
    private readonly menuPositionController;
    /**
     * The menu items associated with this menu. The items must be `MenuItem`s and
     * have both the `md-menu-item` and `md-list-item` attributes.
     */
    get items(): MenuItem[];
    protected willUpdate(changed: PropertyValues<Menu>): void;
    update(changed: PropertyValues<Menu>): void;
    private readonly onWindowResize;
    connectedCallback(): void;
    disconnectedCallback(): void;
    getBoundingClientRect(): DOMRect;
    getClientRects(): DOMRectList;
    protected render(): import("lit-html").TemplateResult<1>;
    /**
     * Renders the positionable surface element and its contents.
     */
    private renderSurface;
    /**
     * Renders the menu items' slot
     */
    private renderMenuItems;
    /**
     * Renders the elevation component.
     */
    private renderElevation;
    private getSurfaceClasses;
    private readonly handleFocusout;
    private captureKeydown;
    /**
     * Saves the last focused element focuses the new element based on
     * `defaultFocus`, and animates open.
     */
    private readonly onOpened;
    /**
     * Animates closed.
     */
    private readonly beforeClose;
    /**
     * Focuses the last focused element.
     */
    private readonly onClosed;
    /**
     * Performs the opening animation:
     *
     * https://direct.googleplex.com/#/spec/295000003+271060003
     *
     * @return A promise that resolve to `true` if the animation was aborted,
     *     `false` if it was not aborted.
     */
    private animateOpen;
    /**
     * Performs the closing animation:
     *
     * https://direct.googleplex.com/#/spec/295000003+271060003
     */
    private animateClose;
    private handleKeydown;
    private setUpGlobalEventListeners;
    private cleanUpGlobalEventListeners;
    private readonly onWindowPointerdown;
    /**
     * We cannot listen to window click because Safari on iOS will not bubble a
     * click event on window if the item clicked is not a "clickable" item such as
     * <body>
     */
    private readonly onDocumentClick;
    private onCloseMenu;
    private onDeactivateItems;
    private onRequestActivation;
    private handleDeactivateTypeahead;
    private handleActivateTypeahead;
    private handleStayOpenOnFocusout;
    private handleCloseOnFocusout;
    close(): void;
    show(): void;
    /**
     * Activates the next item in the menu. If at the end of the menu, the first
     * item will be activated.
     *
     * @return The activated menu item or `null` if there are no items.
     */
    activateNextItem(): MenuItem;
    /**
     * Activates the previous item in the menu. If at the start of the menu, the
     * last item will be activated.
     *
     * @return The activated menu item or `null` if there are no items.
     */
    activatePreviousItem(): MenuItem;
    /**
     * Repositions the menu if it is open.
     *
     * Useful for the case where document or window-positioned menus have their
     * anchors moved while open.
     */
    reposition(): void;
}
