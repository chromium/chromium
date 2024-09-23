/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
import { LitElement, PropertyValues } from 'lit';
import { getFormValue } from '../../labs/behaviors/form-associated.js';
declare const sliderBaseClass: import("../../labs/behaviors/mixin.js").MixinReturn<import("../../labs/behaviors/mixin.js").MixinReturn<(abstract new (...args: any[]) => import("../../labs/behaviors/element-internals.js").WithElementInternals) & typeof LitElement & import("../../labs/behaviors/form-associated.js").FormAssociatedConstructor, import("../../labs/behaviors/form-associated.js").FormAssociated>>;
/**
 * Slider component.
 *
 *
 * @fires change {Event} The native `change` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/change_event)
 * --bubbles
 * @fires input {InputEvent} The native `input` event on
 * [`<input>`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/input_event)
 * --bubbles --composed
 */
export declare class Slider extends sliderBaseClass {
    /** @nocollapse */
    static shadowRootOptions: ShadowRootInit;
    /**
     * The slider minimum value
     */
    min: number;
    /**
     * The slider maximum value
     */
    max: number;
    /**
     * The slider value displayed when range is false.
     */
    value?: number;
    /**
     * The slider start value displayed when range is true.
     */
    valueStart?: number;
    /**
     * The slider end value displayed when range is true.
     */
    valueEnd?: number;
    /**
     * An optional label for the slider's value displayed when range is
     * false; if not set, the label is the value itself.
     */
    valueLabel: string;
    /**
     * An optional label for the slider's start value displayed when
     * range is true; if not set, the label is the valueStart itself.
     */
    valueLabelStart: string;
    /**
     * An optional label for the slider's end value displayed when
     * range is true; if not set, the label is the valueEnd itself.
     */
    valueLabelEnd: string;
    /**
     * Aria label for the slider's start handle displayed when
     * range is true.
     */
    ariaLabelStart: string;
    /**
     * Aria value text for the slider's start value displayed when
     * range is true.
     */
    ariaValueTextStart: string;
    /**
     * Aria label for the slider's end handle displayed when
     * range is true.
     */
    ariaLabelEnd: string;
    /**
     * Aria value text for the slider's end value displayed when
     * range is true.
     */
    ariaValueTextEnd: string;
    /**
     * The step between values.
     */
    step: number;
    /**
     * Whether or not to show tick marks.
     */
    ticks: boolean;
    /**
     * Whether or not to show a value label when activated.
     */
    labeled: boolean;
    /**
     * Whether or not to show a value range. When false, the slider displays
     * a slideable handle for the value property; when true, it displays
     * slideable handles for the valueStart and valueEnd properties.
     */
    range: boolean;
    /**
     * The HTML name to use in form submission for a range slider's starting
     * value. Use `name` instead if both the start and end values should use the
     * same name.
     */
    get nameStart(): string;
    set nameStart(name: string);
    /**
     * The HTML name to use in form submission for a range slider's ending value.
     * Use `name` instead if both the start and end values should use the same
     * name.
     */
    get nameEnd(): string;
    set nameEnd(name: string);
    private readonly inputStart;
    private readonly handleStart;
    private readonly rippleStart;
    private readonly inputEnd;
    private readonly handleEnd;
    private readonly rippleEnd;
    private handleStartHover;
    private handleEndHover;
    private startOnTop;
    private handlesOverlapping;
    private renderValueStart?;
    private renderValueEnd?;
    private get renderAriaLabelStart();
    private get renderAriaValueTextStart();
    private get renderAriaLabelEnd();
    private get renderAriaValueTextEnd();
    private ripplePointerId;
    private isRedispatchingEvent;
    private action?;
    constructor();
    focus(): void;
    protected willUpdate(changed: PropertyValues): void;
    protected updated(changed: PropertyValues): void;
    protected render(): import("lit-html").TemplateResult<1>;
    private renderTrack;
    private renderLabel;
    private renderHandle;
    private renderInput;
    private toggleRippleHover;
    private handleFocus;
    private startAction;
    private finishAction;
    private handleKeydown;
    private handleKeyup;
    private handleDown;
    private handleUp;
    /**
     * The move handler tracks handle hovering to facilitate proper ripple
     * behavior on the slider handle. This is needed because user interaction with
     * the native input is leveraged to position the handle. Because the separate
     * displayed handle element has pointer events disabled (to allow interaction
     * with the input) and the input's handle is a pseudo-element, neither can be
     * the ripple's interactive element. Therefore the input is the ripple's
     * interactive element and has a `ripple` directive; however the ripple
     * is gated on the handle being hovered. In addition, because the ripple
     * hover state is being specially handled, it must be triggered independent
     * of the directive. This is done based on the hover state when the
     * slider is updated.
     */
    private handleMove;
    private handleEnter;
    private handleLeave;
    private updateOnTop;
    private needsClamping;
    private isActionFlipped;
    private flipAction;
    private clampAction;
    private handleInput;
    private handleChange;
    disabled: boolean;
    name: string;
    [getFormValue](): string | FormData;
    formResetCallback(): void;
    formStateRestoreCallback(state: string | Array<[string, string]> | null): void;
}
export {};
