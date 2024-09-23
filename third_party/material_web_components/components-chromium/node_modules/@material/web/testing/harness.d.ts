/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Retrieves the element type from a `Harness` type.
 *
 * @template H The harness type.
 */
export type HarnessElement<H extends Harness> = H extends Harness<infer E> ? ElementWithHarness<E, H> : never;
/**
 * Harnesses will attach themselves to their element for convenience.
 *
 * @template E The element type.
 * @template H The harness type.
 */
export type ElementWithHarness<E extends HTMLElement = HTMLElement, H extends Harness<E> = Harness<E>> = E & {
    /**
     * The harness for this element.
     */
    harness: H;
    /**
     * Associated form element.
     */
    form?: HTMLFormElement | null;
};
/**
 * Checks whether or not an element has a Harness attached to it on the
 * `element.harness` property.
 *
 * @param element The element to check.
 * @return True if the element has a harness property.
 */
export declare function isElementWithHarness(element: Element): element is ElementWithHarness;
/**
 * A test harness class that can be used to simulate interaction with an
 * element.
 *
 * @template E The harness's element type.
 */
export declare class Harness<E extends HTMLElement = HTMLElement> {
    /**
     * The pseudo classes that should be transformed for simulation. Component
     * subclasses may override this to add additional pseudo classes.
     */
    protected transformPseudoClasses: string[];
    /**
     * The element that this harness controls.
     */
    readonly element: E & ElementWithHarness<E, this>;
    /**
     * A set of elements that have already been patched to support transformed
     * pseudo classes.
     */
    private readonly patchedElements;
    /**
     * Creates a new harness for the given element.
     *
     * @param element The element that this harness controls.
     */
    constructor(element: E);
    /**
     * Resets the element's simulated classes to the default state.
     */
    reset(): Promise<void>;
    /**
     * Hovers and clicks on an element. This will generate a `click` event.
     *
     * @param init Additional event options.
     */
    clickWithMouse(init?: PointerEventInit): Promise<void>;
    /**
     * Begins a click with a mouse. Use this along with `endClickWithMouse()` to
     * customize the length of the click.
     *
     * @param init Additional event options.
     */
    startClickWithMouse(init?: PointerEventInit): Promise<void>;
    /**
     * Finishes a click with a mouse. Use this along with `startClickWithMouse()`
     * to customize the length of the click. This will generate a `click` event.
     *
     * @param init Additional event options.
     */
    endClickWithMouse(init?: PointerEventInit): Promise<void>;
    /**
     * Clicks an element with the keyboard (defaults to spacebar). This will
     * generate a `click` event.
     *
     * @param init Additional event options.
     */
    clickWithKeyboard(init?: KeyboardEventInit): Promise<void>;
    /**
     * Begins a click with the keyboard (defaults to spacebar). Use this along
     * with `endClickWithKeyboard()` to customize the length of the click.
     *
     * @param init Additional event options.
     */
    startClickWithKeyboard(init?: KeyboardEventInit): Promise<void>;
    /**
     * Finishes a click with the keyboard (defaults to spacebar). Use this along
     * with `startClickWithKeyboard()` to customize the length of the click.
     *
     * @param init Additional event options.
     */
    endClickWithKeyboard(init?: KeyboardEventInit): Promise<void>;
    /**
     * Right-clicks and opens a context menu. This will generate a `contextmenu`
     * event.
     */
    rightClickWithMouse(): Promise<void>;
    /**
     * Taps once on the element with a simulated touch. This will generate a
     * `click` event.
     *
     * @param init Additional event options.
     * @param touchInit Additional touch event options.
     */
    tap(init?: PointerEventInit, touchInit?: TouchEventInit): Promise<void>;
    /**
     * Begins a touch tap. Use this along with `endTap()` to customize the length
     * or number of taps.
     *
     * @param init Additional event options.
     * @param touchInit Additional touch event options.
     */
    startTap(init?: PointerEventInit, touchInit?: TouchEventInit): Promise<void>;
    /**
     * Simulates a `contextmenu` event for touch. Use this along with `startTap()`
     * to generate a tap-and-hold context menu interaction.
     *
     * @param init Additional event options.
     */
    startTapContextMenu(init?: MouseEventInit): Promise<void>;
    /**
     * Finished a touch tap. Use this along with `startTap()` to customize the
     * length or number of taps.
     *
     * This will NOT generate a `click` event.
     *
     * @param init Additional event options.
     * @param touchInit Additional touch event options.
     */
    endTap(init?: PointerEventInit, touchInit?: TouchEventInit): Promise<void>;
    /**
     * Simulates a `click` event for touch. Use this along with `endTap()` to
     * control the timing of tap and click events.
     *
     * @param init Additional event options.
     */
    endTapClick(init?: PointerEventInit): Promise<void>;
    /**
     * Cancels a touch tap.
     *
     * @param init Additional event options.
     * @param touchInit Additional touch event options.
     */
    cancelTap(init?: PointerEventInit, touchInit?: TouchEventInit): Promise<void>;
    /**
     * Hovers over the element with a simulated mouse.
     */
    startHover(): Promise<void>;
    /**
     * Moves the simulated mouse cursor off of the element.
     */
    endHover(): Promise<void>;
    /**
     * Simulates focusing an element with the keyboard.
     *
     * @param init Additional event options.
     */
    focusWithKeyboard(init?: KeyboardEventInit): Promise<void>;
    /**
     * Simulates focusing an element with a pointer.
     */
    focusWithPointer(): Promise<void>;
    /**
     * Simulates unfocusing an element.
     */
    blur(): Promise<void>;
    /**
     * Simulates a keypress on an element.
     *
     * @param key The key to press.
     * @param init Additional event options.
     */
    keypress(key: string, init?: KeyboardEventInit): Promise<void>;
    /**
     * Simulates submitting the element's associated form element.
     *
     * @param form (Optional) form to submit, defaults to the elemnt's form.
     * @return The submitted form data or null if the element has no associated
     * form.
     */
    submitForm(form?: HTMLFormElement): FormData | Promise<FormData>;
    /**
     * Returns the element that should be used for interaction simulation.
     * Defaults to the host element itself.
     *
     * Subclasses should override this if the interactive element is not the host.
     *
     * @return The element to use in simulation.
     */
    protected getInteractiveElement(): Promise<HTMLElement>;
    /**
     * Adds a pseudo class to an element. The element's shadow root styles (or
     * document if not in a shadow root) will be transformed to support
     * simulated pseudo classes.
     *
     * @param element The element to add a pseudo class to.
     * @param pseudoClass The pseudo class to add.
     */
    protected addPseudoClass(element: HTMLElement, pseudoClass: string): void;
    /**
     * Removes a pseudo class from an element.
     *
     * @param element The element to remove a pseudo class from.
     * @param pseudoClass The pseudo class to remove.
     */
    protected removePseudoClass(element: HTMLElement, pseudoClass: string): void;
    /**
     * Simulates a click event.
     *
     * @param element The element to click.
     * @param init Additional event options.
     */
    protected simulateClick(element: HTMLElement, init?: MouseEventInit): void;
    /**
     * Simulates a contextmenu event.
     *
     * @param element The element to generate an event for.
     * @param init Additional event options.
     */
    protected simulateContextmenu(element: HTMLElement, init?: MouseEventInit): void;
    /**
     * Simulates focusing with a keyboard. The difference between this and
     * `simulatePointerFocus` is that keyboard focus will include the
     * `:focus-visible` pseudo class.
     *
     * @param element The element to focus with a keyboard.
     */
    protected simulateKeyboardFocus(element: HTMLElement): void;
    /**
     * Simulates focusing with a pointer.
     *
     * @param element The element to focus with a pointer.
     */
    protected simulatePointerFocus(element: HTMLElement): void;
    /**
     * Simulates unfocusing an element.
     *
     * @param element The element to blur.
     */
    protected simulateBlur(element: HTMLElement): void;
    /**
     * Simulates a mouse pointer hovering over an element.
     *
     * @param element The element to hover over.
     * @param init Additional event options.
     */
    protected simulateStartHover(element: HTMLElement, init?: PointerEventInit): void;
    /**
     * Simulates a mouse pointer leaving the element.
     *
     * @param element The element to stop hovering over.
     * @param init Additional event options.
     */
    protected simulateEndHover(element: HTMLElement, init?: PointerEventInit): void;
    /**
     * Simulates a mouse press and hold on an element.
     *
     * @param element The element to press with a mouse.
     * @param init Additional event options.
     */
    protected simulateMousePress(element: HTMLElement, init?: PointerEventInit): void;
    /**
     * Simulates a mouse press release from an element.
     *
     * @param element The element to release pressing from.
     * @param init Additional event options.
     */
    protected simulateMouseRelease(element: HTMLElement, init?: PointerEventInit): void;
    /**
     * Simulates a touch press and hold on an element.
     *
     * @param element The element to press with a touch pointer.
     * @param init Additional event options.
     */
    protected simulateTouchPress(element: HTMLElement, init?: PointerEventInit, touchInit?: TouchEventInit): void;
    /**
     * Simulates a touch press release from an element.
     *
     * @param element The element to release pressing from.
     * @param init Additional event options.
     */
    protected simulateTouchRelease(element: HTMLElement, init?: PointerEventInit, touchInit?: TouchEventInit): void;
    /**
     * Simulates a touch cancel from an element.
     *
     * @param element The element to cancel a touch for.
     * @param init Additional event options.
     */
    protected simulateTouchCancel(element: HTMLElement, init?: PointerEventInit, touchInit?: TouchEventInit): void;
    /**
     * Simulates a keypress on an element.
     *
     * @param element The element to press a key on.
     * @param key The key to press.
     * @param init Additional event options.
     */
    protected simulateKeypress(element: EventTarget, key: string, init?: KeyboardEventInit): void;
    /**
     * Simulates a keydown press on an element.
     *
     * @param element The element to press a key on.
     * @param key The key to press.
     * @param init Additional event options.
     */
    protected simulateKeydown(element: EventTarget, key: string, init?: KeyboardEventInit): void;
    /**
     * Simulates a keyup release from an element.
     *
     * @param element The element to release a key from.
     * @param key The key to release.
     * @param init Additional keyboard options.
     */
    protected simulateKeyup(element: EventTarget, key: string, init?: KeyboardEventInit): void;
    /**
     * Creates a MouseEventInit for an element. The default x/y coordinates of the
     * event init will be in the center of the element.
     *
     * @param element The element to create a `MouseEventInit` for.
     * @return The init object for a `MouseEvent`.
     */
    protected createMouseEventInit(element: HTMLElement): MouseEventInit;
    /**
     * Creates a Touch instance for an element. The default x/y coordinates of the
     * touch will be in the center of the element. This can be used in the
     * `TouchEvent` constructor.
     *
     * @param element The element to create a touch for.
     * @param identifier Optional identifier for the touch. Defaults to 0 for
     *     every touch instance.
     * @return The `Touch` instance.
     */
    protected createTouch(element: HTMLElement, identifier?: number): Touch;
    /**
     * Visit each node up the parent tree from the given child until reaching the
     * given parent.
     *
     * This is used to perform logic such as adding/removing recursive pseudo
     * classes like `:hover`.
     *
     * @param child The first child element to start from.
     * @param callback A callback that is invoked with each `HTMLElement` node
     *     from the child to the parent.
     * @param parent The last parent element to visit.
     */
    protected forEachNodeFrom(child: HTMLElement, callback: (node: HTMLElement) => void, parent?: HTMLElement): void;
    /**
     * Patch an element's methods, such as `querySelector` and `matches` to
     * handle transformed pseudo classes.
     *
     * For example, `element.matches(':focus')` will return true when the
     * `._focus` class is applied.
     *
     * @param element The element to patch.
     */
    protected patchForTransformedPseudoClasses(element: HTMLElement): void;
}
