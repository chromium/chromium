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
import { EventType, SpecificEventListener } from '@material/base/types';
import { MDCRipplePoint } from './types';
/**
 * Defines the shape of the adapter expected by the foundation.
 * Implement this adapter for your framework of choice to delegate updates to
 * the component in your framework of choice. See architecture documentation
 * for more details.
 * https://github.com/material-components/material-components-web/blob/master/docs/code/architecture.md
 */
export interface MDCRippleAdapter {
    browserSupportsCssVars(): boolean;
    isUnbounded(): boolean;
    isSurfaceActive(): boolean;
    isSurfaceDisabled(): boolean;
    addClass(className: string): void;
    removeClass(className: string): void;
    containsEventTarget(target: EventTarget | null): boolean;
    registerInteractionHandler<K extends EventType>(evtType: K, handler: SpecificEventListener<K>): void;
    deregisterInteractionHandler<K extends EventType>(evtType: K, handler: SpecificEventListener<K>): void;
    registerDocumentInteractionHandler<K extends EventType>(evtType: K, handler: SpecificEventListener<K>): void;
    deregisterDocumentInteractionHandler<K extends EventType>(evtType: K, handler: SpecificEventListener<K>): void;
    registerResizeHandler(handler: SpecificEventListener<'resize'>): void;
    deregisterResizeHandler(handler: SpecificEventListener<'resize'>): void;
    updateCssVariable(varName: string, value: string | null): void;
    computeBoundingRect(): DOMRect;
    getWindowPageOffset(): MDCRipplePoint;
}
