/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../menu/menu.js';
import { LitElement, PropertyValues } from 'lit';
import { StaticValue } from 'lit/static-html.js';
import { Field } from '../../field/internal/field.js';
import { createValidator, getValidityAnchor } from '../../labs/behaviors/constraint-validation.js';
import { getFormValue } from '../../labs/behaviors/form-associated.js';
import { onReportValidity } from '../../labs/behaviors/on-report-validity.js';
import { SelectValidator } from '../../labs/behaviors/validators/select-validator.js';
import { SelectOption } from './selectoption/select-option.js';
declare const VALUE: unique symbol;
declare const selectBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>, import("../../labs/behaviors/constraint-validation.js").ConstraintValidation>, import("../../labs/behaviors/on-report-validity.js").OnReportValidity>>;
/**
 * @fires change {Event} The native `change` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/change_event)
 * --bubbles
 * @fires input {InputEvent} The native `input` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/input_event)
 * --bubbles --composed
 * @fires opening {Event} Fired when the select's menu is about to open.
 * @fires opened {Event} Fired when the select's menu has finished animations
 * and opened.
 * @fires closing {Event} Fired when the select's menu is about to close.
 * @fires closed {Event} Fired when the select's menu has finished animations
 * and closed.
 */
export declare abstract class Select extends selectBaseClass {
    /** @nocollapse */
    static shadowRootOptions: {
        delegatesFocus: boolean;
        mode: ShadowRootMode;
        slotAssignment?: SlotAssignmentMode;
    };
    /**
     * Opens the menu synchronously with no animation.
     */
    quick: boolean;
    /**
     * Whether or not the select is required.
     */
    required: boolean;
    /**
     * The error message that replaces supporting text when `error` is true. If
     * `errorText` is an empty string, then the supporting text will continue to
     * show.
     *
     * This error message overrides the error message displayed by
     * `reportValidity()`.
     */
    errorText: string;
    /**
     * The floating label for the field.
     */
    label: string;
    /**
     * Disables the asterisk on the floating label, when the select is
     * required.
     */
    noAsterisk: boolean;
    /**
     * Conveys additional information below the select, such as how it should
     * be used.
     */
    supportingText: string;
    /**
     * Gets or sets whether or not the select is in a visually invalid state.
     *
     * This error state overrides the error state controlled by
     * `reportValidity()`.
     */
    error: boolean;
    /**
     * Whether or not the underlying md-menu should be position: fixed to display
     * in a top-level manner, or position: absolute.
     *
     * position:fixed is useful for cases where select is inside of another
     * element with stacking context and hidden overflows such as `md-dialog`.
     */
    menuPositioning: 'absolute' | 'fixed' | 'popover';
    /**
     * Clamps the menu-width to the width of the select.
     */
    clampMenuWidth: boolean;
    /**
     * The max time between the keystrokes of the typeahead select / menu behavior
     * before it clears the typeahead buffer.
     */
    typeaheadDelay: number;
    /**
     * Whether or not the text field has a leading icon. Used for SSR.
     */
    hasLeadingIcon: boolean;
    /**
     * Text to display in the field. Only set for SSR.
     */
    displayText: string;
    /**
     * Whether the menu should be aligned to the start or the end of the select's
     * textbox.
     */
    menuAlign: 'start' | 'end';
    /**
     * The value of the currently selected option.
     *
     * Note: For SSR, set `[selected]` on the requested option and `displayText`
     * rather than setting `value` setting `value` will incur a DOM query.
     */
    get value(): string;
    set value(value: string);
    [VALUE]: string;
    get options(): SelectOption[];
    /**
     * The index of the currently selected option.
     *
     * Note: For SSR, set `[selected]` on the requested option and `displayText`
     * rather than setting `selectedIndex` setting `selectedIndex` will incur a
     * DOM query.
     */
    get selectedIndex(): number;
    set selectedIndex(index: number);
    /**
     * Returns an array of selected options.
     *
     * NOTE: md-select only supports single selection.
     */
    get selectedOptions(): SelectOption[];
    protected abstract readonly fieldTag: StaticValue;
    /**
     * Used for initializing select when the user sets the `value` directly.
     */
    private lastUserSetValue;
    /**
     * Used for initializing select when the user sets the `selectedIndex`
     * directly.
     */
    private lastUserSetSelectedIndex;
    /**
     * Used for `input` and `change` event change detection.
     */
    private lastSelectedOption;
    private lastSelectedOptionRecords;
    /**
     * Whether or not a native error has been reported via `reportValidity()`.
     */
    private nativeError;
    /**
     * The validation message displayed from a native error via
     * `reportValidity()`.
     */
    private nativeErrorText;
    private get hasError();
    private focused;
    private open;
    private defaultFocus;
    private readonly field;
    private readonly menu;
    private readonly labelEl;
    private readonly leadingIcons;
    private prevOpen;
    private selectWidth;
    constructor();
    /**
     * Selects an option given the value of the option, and updates MdSelect's
     * value.
     */
    select(value: string): void;
    /**
     * Selects an option given the index of the option, and updates MdSelect's
     * value.
     */
    selectIndex(index: number): void;
    /**
     * Reset the select to its default value.
     */
    reset(): void;
    [onReportValidity](invalidEvent: Event | null): void;
    protected update(changed: PropertyValues<Select>): void;
    protected render(): import("lit-html").TemplateResult<1>;
    protected firstUpdated(changed: PropertyValues<Select>): Promise<void>;
    private getRenderClasses;
    private renderField;
    private renderFieldContent;
    private renderLeadingIcon;
    private renderTrailingIcon;
    private renderLabel;
    private renderMenu;
    private renderMenuContent;
    /**
     * Handles opening the select on keydown and typahead selection when the menu
     * is closed.
     */
    private handleKeydown;
    private handleClick;
    private handleFocus;
    private handleBlur;
    /**
     * Handles closing the menu when the focus leaves the select's subtree.
     */
    private handleFocusout;
    /**
     * Gets a list of all selected select options as a list item record array.
     *
     * @return An array of selected list option records.
     */
    private getSelectedOptions;
    getUpdateComplete(): Promise<boolean>;
    /**
     * Gets the selected options from the DOM, and updates the value and display
     * text to the first selected option's value and headline respectively.
     *
     * @return Whether or not the selected option has changed since last update.
     */
    private updateValueAndDisplayText;
    /**
     * Focuses and activates the last selected item upon opening, and resets other
     * active items.
     */
    private handleOpening;
    private redispatchEvent;
    private handleClosed;
    /**
     * Determines the reason for closing, and updates the UI accordingly.
     */
    private handleCloseMenu;
    /**
     * Selects a given option, deselects other options, and updates the UI.
     *
     * @return Whether the last selected option has changed.
     */
    private selectItem;
    /**
     * Handles updating selection when an option element requests selection via
     * property / attribute change.
     */
    private handleRequestSelection;
    /**
     * Handles updating selection when an option element requests deselection via
     * property / attribute change.
     */
    private handleRequestDeselection;
    /**
     * Attempts to initialize the selected option from user-settable values like
     * SSR, setting `value`, or `selectedIndex` at startup.
     */
    private initUserSelection;
    private handleIconChange;
    /**
     * Dispatches the `input` and `change` events.
     */
    private dispatchInteractionEvents;
    private getErrorText;
    disabled: boolean;
    name: string;
    [getFormValue](): string;
    formResetCallback(): void;
    formStateRestoreCallback(state: string): void;
    click(): void;
    [createValidator](): SelectValidator;
    [getValidityAnchor](): Field;
}
export {};
