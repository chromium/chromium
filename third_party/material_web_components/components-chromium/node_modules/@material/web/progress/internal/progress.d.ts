/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement, TemplateResult } from 'lit';
declare const progressBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<typeof LitElement>;
/**
 * A progress component.
 */
export declare abstract class Progress extends progressBaseClass {
    /**
     * Progress to display, a fraction between 0 and `max`.
     */
    value: number;
    /**
     * Maximum progress to display, defaults to 1.
     */
    max: number;
    /**
     * Whether or not to display indeterminate progress, which gives no indication
     * to how long an activity will take.
     */
    indeterminate: boolean;
    /**
     * Whether or not to render indeterminate mode using 4 colors instead of one.
     */
    fourColor: boolean;
    protected render(): TemplateResult<1>;
    protected getRenderClasses(): {
        indeterminate: boolean;
        'four-color': boolean;
    };
    protected abstract renderIndicator(): TemplateResult;
}
export {};
