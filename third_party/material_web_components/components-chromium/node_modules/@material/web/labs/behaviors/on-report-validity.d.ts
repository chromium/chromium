/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
import { ConstraintValidation } from './constraint-validation.js';
import { WithElementInternals } from './element-internals.js';
import { MixinBase, MixinReturn } from './mixin.js';
/**
 * A constraint validation element that has a callback for when the element
 * should report validity styles and error messages to the user.
 *
 * This is commonly used in text-field-like controls that display error styles
 * and error messages.
 */
export interface OnReportValidity extends ConstraintValidation {
    /**
     * A callback that is invoked when validity should be reported. Components
     * that can display their own error state can use this and update their
     * styles.
     *
     * If an invalid event is provided, the element is invalid. If `null`, the
     * element is valid.
     *
     * The invalid event's `preventDefault()` may be called to stop the platform
     * popup from displaying.
     *
     * @param invalidEvent The `invalid` event dispatched when an element is
     *     invalid, or `null` if the element is valid.
     */
    [onReportValidity](invalidEvent: Event | null): void;
    formAssociatedCallback(form: HTMLFormElement | null): void;
}
/**
 * A symbol property used for a callback when validity has been reported.
 */
export declare const onReportValidity: unique symbol;
/**
 * Mixes in a callback for constraint validation when validity should be
 * styled and reported to the user.
 *
 * This is commonly used in text-field-like controls that display error styles
 * and error messages.
 *
 * @example
 * ```ts
 * const baseClass = mixinOnReportValidity(
 *   mixinConstraintValidation(
 *     mixinFormAssociated(mixinElementInternals(LitElement)),
 *   ),
 * );
 *
 * class MyField extends baseClass {
 *   \@property({type: Boolean}) error = false;
 *   \@property() errorMessage = '';
 *
 *   [onReportValidity](invalidEvent: Event | null) {
 *     this.error = !!invalidEvent;
 *     this.errorMessage = this.validationMessage;
 *
 *     // Optionally prevent platform popup from displaying
 *     invalidEvent?.preventDefault();
 *   }
 * }
 * ```
 *
 * @param base The class to mix functionality into.
 * @return The provided class with `OnReportValidity` mixed in.
 */
export declare function mixinOnReportValidity<T extends MixinBase<LitElement & ConstraintValidation & WithElementInternals>>(base: T): MixinReturn<T, OnReportValidity>;
