/**
 * @license
 * Copyright 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
import { MDCFoundation } from '@material/base/foundation';
import { MDCRippleAdapter } from './adapter';
export declare class MDCRippleFoundation extends MDCFoundation<MDCRippleAdapter> {
    static get cssClasses(): {
        BG_FOCUSED: string;
        FG_ACTIVATION: string;
        FG_DEACTIVATION: string;
        ROOT: string;
        UNBOUNDED: string;
    };
    static get strings(): {
        VAR_FG_SCALE: string;
        VAR_FG_SIZE: string;
        VAR_FG_TRANSLATE_END: string;
        VAR_FG_TRANSLATE_START: string;
        VAR_LEFT: string;
        VAR_TOP: string;
    };
    static get numbers(): {
        DEACTIVATION_TIMEOUT_MS: number;
        FG_DEACTIVATION_MS: number;
        INITIAL_ORIGIN_SCALE: number;
        PADDING: number;
        TAP_DELAY_MS: number;
    };
    static get defaultAdapter(): MDCRippleAdapter;
    private activationAnimationHasEnded;
    private activationState;
    private activationTimer;
    private fgDeactivationRemovalTimer;
    private fgScale;
    private frame;
    private initialSize;
    private layoutFrame;
    private maxRadius;
    private unboundedCoords;
    private readonly activationTimerCallback;
    private readonly activateHandler;
    private readonly deactivateHandler;
    private readonly focusHandler;
    private readonly blurHandler;
    private readonly resizeHandler;
    private previousActivationEvent?;
    constructor(adapter?: Partial<MDCRippleAdapter>);
    init(): void;
    destroy(): void;
    /**
     * @param evt Optional event containing position information.
     */
    activate(evt?: Event): void;
    deactivate(): void;
    layout(): void;
    setUnbounded(unbounded: boolean): void;
    handleFocus(): void;
    handleBlur(): void;
    /**
     * We compute this property so that we are not querying information about the client
     * until the point in time where the foundation requests it. This prevents scenarios where
     * client-side feature-detection may happen too early, such as when components are rendered on the server
     * and then initialized at mount time on the client.
     */
    private supportsPressRipple;
    private defaultActivationState;
    /**
     * supportsPressRipple Passed from init to save a redundant function call
     */
    private registerRootHandlers;
    private registerDeactivationHandlers;
    private deregisterRootHandlers;
    private deregisterDeactivationHandlers;
    private removeCssVars;
    private activateImpl;
    private checkElementMadeActive;
    private animateActivation;
    private getFgTranslationCoordinates;
    private runDeactivationUXLogicIfReady;
    private rmBoundedActivationClasses;
    private resetActivationState;
    private deactivateImpl;
    private animateDeactivation;
    private layoutInternal;
    private updateLayoutCssVars;
}
export default MDCRippleFoundation;
